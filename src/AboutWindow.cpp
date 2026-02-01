/*
 * Sestriere - MeshCore Client for Haiku OS
 * AboutWindow.cpp - Custom About window implementation
 */

#include "AboutWindow.h"

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <Font.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextView.h>
#include <View.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AboutWindow"

static const uint32 kMsgClose = 'clos';


AboutWindow::AboutWindow()
	: BWindow(BRect(0, 0, 380, 280), B_TRANSLATE("About Sestriere"),
		B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
		| B_CLOSE_ON_ESCAPE)
{
	_BuildLayout();
	CenterOnScreen();
}


AboutWindow::~AboutWindow()
{
}


void
AboutWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgClose:
			Quit();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}


void
AboutWindow::_BuildLayout()
{
	// Title
	fTitleView = new BStringView("title", "Sestriere");
	BFont titleFont(be_bold_font);
	titleFont.SetSize(24);
	fTitleView->SetFont(&titleFont);
	fTitleView->SetHighColor(50, 100, 150);

	// Version
	fVersionView = new BStringView("version", B_TRANSLATE("Version 1.0"));
	BFont versionFont(be_plain_font);
	versionFont.SetSize(12);
	fVersionView->SetFont(&versionFont);
	fVersionView->SetHighColor(100, 100, 100);

	// Description
	fDescriptionView = new BTextView("description");
	fDescriptionView->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	fDescriptionView->MakeEditable(false);
	fDescriptionView->MakeSelectable(false);
	fDescriptionView->SetStylable(true);
	fDescriptionView->SetExplicitMinSize(BSize(340, 120));

	const char* description =
		"A native MeshCore LoRa mesh client for Haiku OS.\n\n"
		"The name recalls the Venetian 'sestieri' - interconnected "
		"districts communicating across canals, like nodes in a mesh network.\n\n"
		"Features:\n"
		"  \xE2\x80\xA2 USB serial connection to MeshCore devices\n"
		"  \xE2\x80\xA2 Contact management and messaging\n"
		"  \xE2\x80\xA2 Geographic map visualization\n"
		"  \xE2\x80\xA2 Network topology graph\n"
		"  \xE2\x80\xA2 Sensor telemetry dashboard\n";

	fDescriptionView->SetText(description);

	// Copyright
	BStringView* copyrightView = new BStringView("copyright",
		"Copyright " B_UTF8_COPYRIGHT " 2025 Sestriere Authors");
	BFont copyrightFont(be_plain_font);
	copyrightFont.SetSize(10);
	copyrightView->SetFont(&copyrightFont);
	copyrightView->SetHighColor(80, 80, 80);

	BStringView* licenseView = new BStringView("license",
		B_TRANSLATE("Distributed under the MIT license"));
	licenseView->SetFont(&copyrightFont);
	licenseView->SetHighColor(80, 80, 80);

	// Close button
	fCloseButton = new BButton("close", B_TRANSLATE("Close"),
		new BMessage(kMsgClose));
	fCloseButton->MakeDefault(true);

	// Layout
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
			.Add(fTitleView)
			.Add(fVersionView)
		.End()
		.AddStrut(B_USE_DEFAULT_SPACING)
		.Add(fDescriptionView)
		.AddStrut(B_USE_DEFAULT_SPACING)
		.AddGroup(B_VERTICAL, 0)
			.Add(copyrightView)
			.Add(licenseView)
		.End()
		.AddStrut(B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fCloseButton)
		.End()
	.End();
}
