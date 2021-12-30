#ifndef FUNCTIONVIEW_H
#define FUNCTIONVIEW_H


#include <View.h>
#include <PopUpMenu.h>
#include <MenuField.h>
#include <String.h>
#include <Entry.h>

#include <vector>


struct ctags_tag {
	BString name;
	int32 line_nr;
	BString scope;
};

typedef std::vector<ctags_tag> ctags_vector;

enum {
	FV_SELECTION_CHANGED 	= 'fvsc',
	FV_GOTO_LINE 			= 'fvgl'
};


class FunctionView : public BView {
public:
	FunctionView();
	void SetFile(BEntry file_entry);
	void Reload();
	void MessageReceived(BMessage *msg);

private:
	BString get_ctags_data(BString filename);
	void get_tags(BString ctags_data);

	BPopUpMenu 		*fPopUpMenu;
	BMenuField 		*fMenuField;
	ctags_vector	fTags;
	BString			fFilename;
};


#endif
