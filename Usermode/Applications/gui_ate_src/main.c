/*
 * Acess Text Editor (ATE)
 * - By John Hodge (thePowersGang)
 *
 * main.c
 * - Core
 */
#include <axwin3/axwin.h>
#include <axwin3/widget.h>
#include <axwin3/menu.h>
#include <axwin3/richtext.h>
#include <stdio.h>
#include "strings.h"

// === PROTOTYPES ===
 int	main(int argc, char *argv[]);
 int	TextArea_KeyHandler(tHWND Window, int bPress, uint32_t KeySym, uint32_t Translated);
 int	TextArea_MouseHandler(tHWND Window, int bPress, int Button, int Row, int Col);
void	add_toolbar_button(tAxWin3_Widget *Toolbar, const char *Ident, tAxWin3_Widget_FireCb Callback);
 int	Toolbar_New(tAxWin3_Widget *Widget);
 int	Toolbar_Open(tAxWin3_Widget *Widget);
 int	Toolbar_Save(tAxWin3_Widget *Widget);
 int	Toolbar_Close(tAxWin3_Widget *Widget);

// === GLOBALS ===
tHWND	gMainWindow;
tAxWin3_Widget	*gMainWindow_Root;
tHWND	gMainWindow_MenuBar;
tAxWin3_Widget	*gMainWindow_Toolbar;
tHWND	gMainWindow_TextArea;

// === CODE ===
int main(int argc, char *argv[])
{
	AxWin3_Connect(NULL);
	
	// --- Build up window
	gMainWindow = AxWin3_Widget_CreateWindow(NULL, 500, 400, ELEFLAG_VERTICAL);
	AxWin3_SetWindowTitle(gMainWindow, "Acess Text Editor");	// TODO: Update title with other info
	gMainWindow_Root = AxWin3_Widget_GetRoot(gMainWindow);

	//gMainWindow_MenuBar = AxWin3_Menu_Create(gMainWindow);
	//AxWin3_Widget_AddWidget_SubWindow(gMainWindow_Root, gMainWindow_MenuBar);
	// TODO: Populate menu	

	// Create toolbar
	gMainWindow_Toolbar = AxWin3_Widget_AddWidget(gMainWindow_Root,
		ELETYPE_TOOLBAR, ELEFLAG_NOSTRETCH, "Toolbar");
	add_toolbar_button(gMainWindow_Toolbar, "BtnNew", Toolbar_New);
	add_toolbar_button(gMainWindow_Toolbar, "BtnOpen", Toolbar_Open);
	add_toolbar_button(gMainWindow_Toolbar, "BtnSave", Toolbar_Save);
	add_toolbar_button(gMainWindow_Toolbar, "BtnClose", Toolbar_Close);
	AxWin3_Widget_AddWidget(gMainWindow_Toolbar, ELETYPE_SPACER, ELEFLAG_NOSTRETCH, "");
	add_toolbar_button(gMainWindow_Toolbar, "BtnUndo", NULL);
	add_toolbar_button(gMainWindow_Toolbar, "BtnRedo", NULL);
	AxWin3_Widget_AddWidget(gMainWindow_Toolbar, ELETYPE_SPACER, ELEFLAG_NOSTRETCH, "");
	add_toolbar_button(gMainWindow_Toolbar, "BtnCut", NULL);
	add_toolbar_button(gMainWindow_Toolbar, "BtnCopy", NULL);
	add_toolbar_button(gMainWindow_Toolbar, "BtnPaste", NULL);
	AxWin3_Widget_AddWidget(gMainWindow_Toolbar, ELETYPE_SPACER, ELEFLAG_NOSTRETCH, "");
	add_toolbar_button(gMainWindow_Toolbar, "BtnSearch", NULL);
	add_toolbar_button(gMainWindow_Toolbar, "BtnReplace", NULL);

	// TODO: Tab control?	

	gMainWindow_TextArea = AxWin3_RichText_CreateWindow(gMainWindow, 0);
	AxWin3_Widget_AddWidget_SubWindow(gMainWindow_Root, gMainWindow_TextArea, 0, "TextArea");
	AxWin3_RichText_SetKeyHandler	(gMainWindow_TextArea, TextArea_KeyHandler);
	AxWin3_RichText_SetMouseHandler	(gMainWindow_TextArea, TextArea_MouseHandler);
	AxWin3_RichText_SetBackground	(gMainWindow_TextArea, 0xFFFFFF);
	AxWin3_RichText_SetDefaultColour(gMainWindow_TextArea, 0x000000);
	AxWin3_RichText_SetFont		(gMainWindow_TextArea, "#monospace", 10);
	AxWin3_RichText_SetCursorPos	(gMainWindow_TextArea, 0, 0);
	AxWin3_RichText_SetCursorType	(gMainWindow_TextArea, AXWIN3_RICHTEXT_CURSOR_VLINE);
	AxWin3_RichText_SetCursorBlink	(gMainWindow_TextArea, 1);

	// <testing>
	AxWin3_RichText_SetLineCount(gMainWindow_TextArea, 3);
	AxWin3_RichText_SendLine(gMainWindow_TextArea, 0, "First line!");
	AxWin3_RichText_SendLine(gMainWindow_TextArea, 2, "Third line! \x01""ff0000red\x01""00ff00green");
	// </testing>

	AxWin3_ShowWindow(gMainWindow_TextArea, 1);
	// TODO: Status Bar?

	AxWin3_MoveWindow(gMainWindow, 50, 50);
	AxWin3_ShowWindow(gMainWindow, 1);
	AxWin3_FocusWindow(gMainWindow);
	
	// Main loop
	AxWin3_MainLoop();

	AxWin3_DestroyWindow(gMainWindow);

	return 0;
}

int TextArea_KeyHandler(tHWND Window, int bPress, uint32_t KeySym, uint32_t Translated)
{
	return 0;
}

int TextArea_MouseHandler(tHWND Window, int bPress, int Button, int Row, int Col)
{
	return 0;
}

void add_toolbar_button(tAxWin3_Widget *Toolbar, const char *Ident, tAxWin3_Widget_FireCb Callback)
{
	tAxWin3_Widget *btn = AxWin3_Widget_AddWidget(Toolbar, ELETYPE_BUTTON, ELEFLAG_NOSTRETCH, Ident);
	const char *img = getimg(Ident);
	if( img )
	{
		tAxWin3_Widget *txt = AxWin3_Widget_AddWidget(btn, ELETYPE_IMAGE, 0, Ident);
		AxWin3_Widget_SetText(txt, img);
		// TODO: tooltip?
	}
	else
	{
		tAxWin3_Widget *txt = AxWin3_Widget_AddWidget(btn, ELETYPE_TEXT, 0, Ident);
		AxWin3_Widget_SetText(txt, getstr(Ident));
	}
	AxWin3_Widget_SetFireHandler(btn, Callback);
}

int Toolbar_New(tAxWin3_Widget *Widget)
{
	return 0;
}
int Toolbar_Open(tAxWin3_Widget *Widget)
{
	return 0;
}
int Toolbar_Save(tAxWin3_Widget *Widget)
{
	return 0;
}
int Toolbar_Close(tAxWin3_Widget *Widget)
{
	AxWin3_StopMainLoop(1);
	return 0;
}

