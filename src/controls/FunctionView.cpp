#include "FunctionView.h"

#include <MenuItem.h>

#include <LayoutBuilder.h>
#include <Catalog.h>


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
