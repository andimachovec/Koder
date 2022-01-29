#ifndef FUNCTIONVIEW_H
#define FUNCTIONVIEW_H


#include <View.h>
#include <PopUpMenu.h>
#include <Button.h>
#include <String.h>
#include <Entry.h>

#include <vector>


class CtagsTag {
public:	
	bool operator < (const CtagsTag& tag) const;
    BString name;
	int32 line_nr;
	BString scope;
};

typedef std::vector<CtagsTag> ctags_vector;

enum {
	FV_OPEN_MENU			= 'fvom',
	FV_SELECTION_CHANGED 	= 'fvsc',
	FV_GOTO_LINE 			= 'fvgl'
};


class FunctionView : public BView {
public:
	FunctionView();
	void SetFile(BEntry file_entry);
	void Reload();
	void MessageReceived(BMessage *msg);
	void AttachedToWindow();
	
private:
	BString get_ctags_data(BString filename);
	void get_tags(BString ctags_data);
	void sort_tags();

	BPopUpMenu 		*fPopUpMenu;
	BButton 		*fButton;
	ctags_vector	fTags;
	BString			fFilename;
};


#endif
