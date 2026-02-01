/*
 * Sestriere - MeshCore Client for Haiku OS
 * AboutWindow.h - Custom About window
 */

#ifndef ABOUT_WINDOW_H
#define ABOUT_WINDOW_H

#include <Window.h>

class BButton;
class BStringView;
class BTextView;

class AboutWindow : public BWindow {
public:
						AboutWindow();
	virtual				~AboutWindow();

	virtual void		MessageReceived(BMessage* message);

private:
	void				_BuildLayout();

	BStringView*		fTitleView;
	BStringView*		fVersionView;
	BTextView*			fDescriptionView;
	BButton*			fCloseButton;
};

#endif // ABOUT_WINDOW_H
