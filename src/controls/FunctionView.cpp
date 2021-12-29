#include "FunctionView.h"

#include <MenuItem.h>
#include <LayoutBuilder.h>
#include <Catalog.h>
#include <StringList.h>
#include <Path.h>

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
		fPopUpMenu->AddItem(new BMenuItem(tag.name, new BMessage(FV_SELECTION_CHANGED)));
	}

}

BString
FunctionView::get_ctags_data(BString filename)
{

	BString ctags_command;
	ctags_command << "ctags -x " << filename;
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
	std::cout << ctags_data << std::endl;

	fTags.clear();

	//parse ctags output
	BStringList ctags_data_lines;
	ctags_data.Split("\n", true, ctags_data_lines);

	for (int32 line_idx = 0; line_idx < ctags_data_lines.CountStrings(); ++line_idx)
	{
		BString line = ctags_data_lines.StringAt(line_idx);
		BStringList token_list;
		line.Split("\t", false, token_list);

		BString kind = token_list.StringAt(3).RemoveFirst("kind:");
		if (kind == "function")
		{
			ctags_tag tag;
			tag.name = token_list.StringAt(0);
			tag.line_nr = atoi(token_list.StringAt(4).RemoveFirst("line:").String());

			BString scope = token_list.StringAt(5);
			int32 cut_pos = scope.FindLast(":");
			if (cut_pos	> 0)
			{
				tag.scope = scope.Remove(0, cut_pos+1);
			}

			fTags.push_back(tag);
		}

	}

}
