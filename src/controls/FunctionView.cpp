#include "FunctionView.h"

#include <MenuItem.h>
#include <LayoutBuilder.h>
#include <Catalog.h>
#include <StringList.h>
#include <Path.h>

#include <cstdio>
#include <memory>
#include <array>
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

	BString ctags_command = "ctags --sort=no -f - --fields=\"sKNnzZ\" --output-format=\"u-ctags\"";
	BString popen_cmd;
	popen_cmd << ctags_command << " " << filename;
	std::cout << popen_cmd << std::endl;

	popen_cmd = "ls -l";

	std::array<char, 128> buffer;
	BString ctags_data;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(popen_cmd.String(), "r"), pclose);

	if (!pipe)
	{
		while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
		{
			ctags_data << buffer.data();
		}
	}
	else
	{
		std::cout << "Error running ctags..." << std::endl;
		std::cout << "Error: " << errno << std::endl;
	}

	std::cout << ctags_data << std::endl;

	return ctags_data;

}


void
FunctionView::get_tags(BString ctags_data)
{
	std::cout << "ctags_data: " << ctags_data << std::endl;

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
