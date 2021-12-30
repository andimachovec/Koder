#include "FunctionView.h"

#include <MenuItem.h>
#include <LayoutBuilder.h>
#include <Catalog.h>
#include <StringList.h>
#include <Path.h>
#include <Json.h>

#include <cstdio>
#include <array>
#include <unistd.h>
#include <iostream>



#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "FunctionView"


FunctionView::FunctionView()
	:
	BView("functionview", B_SUPPORTS_LAYOUT)
{

	fPopUpMenu = new BPopUpMenu("");
	fMenuField = new BMenuField("",fPopUpMenu);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_SPACING)
		.Add(fMenuField)
	.Layout();

}


void
FunctionView::SetFile(BEntry file_entry)
{
	BPath file_path;
	file_entry.GetPath(&file_path);
	fFilename = file_path.Path();

	Reload();

}


void
FunctionView::Reload()
{

	get_tags(get_ctags_data(fFilename));

	//update menuitems
	int32 item_count = fPopUpMenu->CountItems();

	if (item_count > 0)
	{
		fPopUpMenu->RemoveItems(0, item_count, false);
	}

	ctags_vector::iterator tags_iter;
	for (tags_iter = fTags.begin(); tags_iter != fTags.end(); ++tags_iter)
	{
		ctags_tag tag = *tags_iter;
		BString tag_displayname;
		tag_displayname << tag.scope << "::" << tag.name;
		fPopUpMenu->AddItem(new BMenuItem(tag_displayname, new BMessage(FV_SELECTION_CHANGED)));
	}

	fPopUpMenu->SetTargetForItems(this);

}


void
FunctionView::MessageReceived(BMessage *msg)
{

	switch(msg->what)
	{
		case FV_SELECTION_CHANGED:
		{
			int32 selection_index = fPopUpMenu->FindMarkedIndex();
			int32 line_nr = fTags[selection_index].line_nr;

			BMessage goto_line_message(FV_GOTO_LINE);
			goto_line_message.AddInt32("line", line_nr);
			Window()->PostMessage(&goto_line_message);

			break;
		}

		default:
			BView::MessageReceived(msg);
			break;
	}
}


BString
FunctionView::get_ctags_data(BString filename)
{

	BString ctags_command;
	ctags_command << "ctags -f - --fields=\"sKNnzZ\" --output-format=\"json\" " << filename;
	BString ctags_data;

	int stdout_pipe[2];
	int original_stdout = dup(STDOUT_FILENO);

	if (pipe(stdout_pipe) == 0)
	{
		dup2(stdout_pipe[1], STDOUT_FILENO);
		close(stdout_pipe[1]);
	}

	int pipe_flags = fcntl(stdout_pipe[0], F_GETFD);
	pipe_flags |= FD_CLOEXEC;
	fcntl(stdout_pipe[0], F_SETFD, pipe_flags);

	const char *arguments[4];
	arguments[0] = "/bin/sh";
	arguments[1] = "-c";
	arguments[2] = ctags_command.String();
	arguments[3] = nullptr;

	thread_id newprogram_id = load_image(3, arguments, const_cast<const char **>(environ));
	status_t error_code = newprogram_id;
	if (error_code >= 0)
	{
		setpgid(newprogram_id, newprogram_id);
		error_code = resume_thread(newprogram_id);
	}

	dup2 (original_stdout, STDOUT_FILENO);

	if (error_code >= 0)
	{
		while (true)
		{
			char buffer[4096];
			ssize_t amount_read = read(stdout_pipe[0], buffer, sizeof(buffer));

			if (amount_read <= 0)
			{
				break;
			}

			ctags_data.Append(buffer, amount_read);
		}
	}

	close(stdout_pipe[0]);
	status_t proc_exit_code = 0;
	wait_for_thread(newprogram_id, &proc_exit_code);

	return ctags_data;

}


void
FunctionView::get_tags(BString ctags_data)
{

	fTags.clear();

	BStringList ctags_data_lines;
	ctags_data.Split("\n", true, ctags_data_lines);

	for (int32 line_idx = 0; line_idx < ctags_data_lines.CountStrings(); ++line_idx)
	{
		BMessage json_message;
		BString line = ctags_data_lines.StringAt(line_idx);
		BJson::Parse(line, json_message);

		const char *kind;
		json_message.FindString("kind", &kind);

		if (BString(kind) == "function")
		{
			ctags_tag tag;
			const char *name;
			double line_nr;
			const char *scope;

			json_message.FindString("name", &name);
			tag.name = name;

			json_message.FindDouble("line", &line_nr);
			tag.line_nr = line_nr;

			json_message.FindString("scope", &scope);
			tag.scope = scope;

			fTags.push_back(tag);
		}
	}

}
