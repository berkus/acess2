/*
 * Acess GUI (AxWin) Version 2
 * By John Hodge (thePowersGang)
 * 
 * Widget Decorator
 */
#include "common.h"
#include "wm.h"

#define BORDER_EVERYTHING	1

#define BOX_BGCOLOUR    0xC0C0C0
#define BOX_BORDER      0xA0A0A0
#define BUTTON_BGCOLOUR 0xD0D0D0
#define BUTTON_BORDER   0xF0F0F0
#define	TEXT_COLOUR     0x000000

// === CODE ===
void Decorator_RenderWidget(tElement *Element)
{
	_SysDebug("Decorator_RenderWidget: (Element={Type:%i,(%i,%i) %ix%i})",
		Element->Type,
		Element->CachedX, Element->CachedY,
		Element->CachedW, Element->CachedH
		);
	
	#if BORDER_EVERYTHING
	Video_DrawRect(Element->CachedX, Element->CachedY,
			Element->CachedW, Element->CachedH,
			0
			);
	#endif
	
	switch(Element->Type)
	{
	case ELETYPE_NONE:
	case ELETYPE_BOX:	break;	// Box is a meta-element
	
	case ELETYPE_TABBAR:	// Tab Bar
	case ELETYPE_TOOLBAR:	// Tool Bar
		Video_DrawRect(
			Element->CachedX, Element->CachedY,
			Element->CachedW, Element->CachedH,
			BOX_BORDER
			);
		Video_FillRect(
			Element->CachedX+1, Element->CachedY+1,
			Element->CachedW-2, Element->CachedH-2,
			BOX_BGCOLOUR
			);
		break;
	
	case ELETYPE_SPACER:	// Spacer (subtle line)
		Video_FillRect(
			Element->CachedX+3, Element->CachedY+3,
			Element->CachedW-6, Element->CachedH-6,
			BOX_BORDER
			);
		break;
	
	case ELETYPE_BUTTON:	// Button
		Video_FillRect(
			Element->CachedX+1, Element->CachedY+1,
			Element->CachedW-2, Element->CachedH-2,
			BUTTON_BGCOLOUR
			);
		Video_DrawRect(
			Element->CachedX, Element->CachedY,
			Element->CachedW-1, Element->CachedH-1,
			BUTTON_BORDER
			);
		break;
	
	case ELETYPE_TEXT:
		Video_DrawText(
			Element->CachedX+1, Element->CachedY+1,
			Element->CachedW-2, Element->CachedH-2,
			NULL,
			TEXT_COLOUR,
			Element->Text
			);
		break;
	
	case ELETYPE_IMAGE:
		Video_DrawImage(
			Element->CachedX, Element->CachedY,
			Element->CachedW, Element->CachedH,
			Element->Data
			);
		break;
		
	default:
		_SysDebug(" ERROR: Unknown type %i", Element->Type);
		break;
	}
}
