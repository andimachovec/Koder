#ifndef FUNCTIONVIEW_H
#define FUNCTIONVIEW_H


#include <View.h>
#include <PopUpMenu.h>
#include <MenuField.h>


enum {
	FV_SELECTION_CHANGED = 'fv00',
};


class FunctionView : public BView {
public:
	FunctionView();

private:
	BPopUpMenu *fPopUpMenu;
	BMenuField *fMenuField;
};


#endif
