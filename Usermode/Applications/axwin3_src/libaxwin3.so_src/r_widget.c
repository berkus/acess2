/*
 * AxWin3 Interface Library
 * - By John Hodge (thePowersGang)
 *
 * r_widget.c
 * - Widget window type interface
 */
#include <axwin3/axwin.h>
#include <axwin3/widget.h>
#include "include/internal.h"
#include <stdlib.h>
#include <widget_messages.h>
#include <string.h>

//static const int	ciBaseElementCount = 16;
static const int	ciStepElementCount = 16;

// === STRUCTURES ===
struct sAxWin3_Widget
{
	tHWND	Window;
	uint32_t	ID;
	// Callbacks
	tAxWin3_Widget_FireCb	Fire;
	tAxWin3_Widget_KeyUpDownCb	KeyUpDown;
	tAxWin3_Widget_KeyFireCb	KeyFire;
	tAxWin3_Widget_MouseMoveCb	MouseMove;
	tAxWin3_Widget_MouseBtnCb	MouseButton;
};

typedef struct
{
	 int	nElements;
	tAxWin3_Widget	**Elements;
	 int	FirstFreeID;
	tAxWin3_Widget	RootElement;
	// Callbacks for each element
} tWidgetWindowInfo;

// === CODE ===
tAxWin3_Widget *AxWin3_Widget_int_GetElementByID(tHWND Window, uint32_t ID)
{
	tWidgetWindowInfo	*info;
	if(!Window)	return NULL;
	
	info = AxWin3_int_GetDataPtr(Window);
	if(ID >= info->nElements)	return NULL;
	
	return info->Elements[ID];
}

uint32_t AxWin3_Widget_int_AllocateID(tWidgetWindowInfo *Info)
{
	uint32_t	newID;
	// BUG BUG BUG - Double Allocations! (citation needed)
	// TODO: Atomicity
	for( newID = Info->FirstFreeID; newID < Info->nElements; newID ++ )
	{
		if( Info->Elements[newID] == NULL )
			break;
	}
	if( newID == Info->nElements )
	{
		Info->nElements += ciStepElementCount;
		Info->Elements = realloc(Info->Elements, sizeof(*Info->Elements)*Info->nElements);
		newID = Info->nElements - ciStepElementCount;
		memset( &Info->Elements[newID+1], 0, (ciStepElementCount-1)*sizeof(Info->Elements));
		_SysDebug("Widget: Expanded to %i and allocated %i", Info->nElements, newID);
	}
	else
		_SysDebug("Widget: Allocated %i", newID);
	Info->Elements[newID] = (void*)-1;
	
	return newID;
	
}

int AxWin3_Widget_MessageHandler(tHWND Window, int MessageID, int Size, void *Data)
{
	tAxWin3_Widget	*widget;

	switch(MessageID)
	{
//	case WNDMSG_DESTROY: {
//		return 0; }
	case MSG_WIDGET_FIRE: {
		tWidgetMsg_Fire	*msg = Data;
		if(Size < sizeof(*msg))	return -1;
		widget = AxWin3_Widget_int_GetElementByID(Window, msg->WidgetID);
		if(widget->Fire)	widget->Fire(widget);
		
		return 1; }
	case MSG_WIDGET_KEYPRESS: {
		return 0; }
	case MSG_WIDGET_MOUSEBTN: {
		// TODO: Do something
		return 1; }
	default:
		return 0;
	}
}

tHWND AxWin3_Widget_CreateWindow(tHWND Parent, int W, int H, int RootEleFlags)
{
	tHWND	ret;
	tWidgetWindowInfo	*info;
	
	ret = AxWin3_CreateWindow(
		Parent, "Widget", RootEleFlags,
		sizeof(*info) + sizeof(tAxWin3_Widget), AxWin3_Widget_MessageHandler
		);
	info = AxWin3_int_GetDataPtr(ret);

	info->RootElement.Window = ret;
	info->RootElement.ID = -1;

	AxWin3_ResizeWindow(ret, W, H);
	
	return ret;
}

void AxWin3_Widget_DestroyWindow(tHWND Window)
{
	// Free all element structures
	
	// Free array
	
	// Request window to be destroyed (will clean up any data stored in tHWND)
}

tAxWin3_Widget *AxWin3_Widget_GetRoot(tHWND Window)
{
	tWidgetWindowInfo	*info = AxWin3_int_GetDataPtr(Window);
	return &info->RootElement;
}

tAxWin3_Widget *AxWin3_Widget_AddWidget(tAxWin3_Widget *Parent, int Type, int Flags, const char *DebugName)
{
	 int	newID;
	tWidgetWindowInfo	*info;
	tAxWin3_Widget	*ret;

	if(!Parent)	return NULL;

	info = AxWin3_int_GetDataPtr(Parent->Window);
	
	newID = AxWin3_Widget_int_AllocateID(info);
	
	// Create new widget structure
	ret = calloc(sizeof(tAxWin3_Widget), 1);
	ret->Window = Parent->Window;
	ret->ID = newID;

	info->Elements[newID] = ret;

	// Send create widget message
	{
		char	tmp[sizeof(tWidgetIPC_Create)+1];
		tWidgetIPC_Create	*msg = (void*)tmp;
		msg->Parent = Parent->ID;
		msg->NewID = newID;
		msg->Type = Type;
		msg->Flags = Flags;
		msg->DebugName[0] = '\0';
		AxWin3_SendIPC(ret->Window, IPC_WIDGET_CREATE, sizeof(tmp), tmp);
	}

	return ret;
}

tAxWin3_Widget *AxWin3_Widget_AddWidget_SubWindow(tAxWin3_Widget *Parent, tHWND Window, int Flags, const char *DbgName)
{
	tWidgetWindowInfo	*info = AxWin3_int_GetDataPtr(Parent->Window);
	int newID = AxWin3_Widget_int_AllocateID(info);
	
	tAxWin3_Widget	*ret = calloc(sizeof(tAxWin3_Widget), 1);
	ret->Window = Parent->Window;
	ret->ID = newID;
	info->Elements[newID] = ret;

	// Send message
	{
		char	tmp[sizeof(tWidgetIPC_CreateSubWin)+1];
		tWidgetIPC_CreateSubWin	*msg = (void*)tmp;
		msg->Parent = Parent->ID;
		msg->NewID = newID;
		msg->Type = ELETYPE_SUBWIN;
		msg->Flags = Flags;	// TODO: Flags
		msg->WindowHandle = AxWin3_int_GetWindowID(Window);
		msg->DebugName[0] = '\0';
		AxWin3_SendIPC(ret->Window, IPC_WIDGET_CREATESUBWIN, sizeof(tmp), tmp);
	}

	return ret;
}


void AxWin3_Widget_DelWidget(tAxWin3_Widget *Widget)
{
	tWidgetIPC_Delete	msg;
	tWidgetWindowInfo	*info = AxWin3_int_GetDataPtr(Widget->Window);
	
	msg.WidgetID = Widget->ID;
	AxWin3_SendIPC(Widget->Window, IPC_WIDGET_DELETE, sizeof(msg), &msg);
	
	info->Elements[Widget->ID] = NULL;
	if(Widget->ID < info->FirstFreeID)
		info->FirstFreeID = Widget->ID;
	free(Widget);
}

// --- Callbacks
void AxWin3_Widget_SetFireHandler(tAxWin3_Widget *Widget, tAxWin3_Widget_FireCb Callback)
{
	if(!Widget)	return;
	Widget->Fire = Callback;
}

void AxWin3_Widget_SetKeyHandler(tAxWin3_Widget *Widget, tAxWin3_Widget_KeyUpDownCb Callback)
{
	if(!Widget)	return;
	Widget->KeyUpDown = Callback;
}

void AxWin3_Widget_SetKeyFireHandler(tAxWin3_Widget *Widget, tAxWin3_Widget_KeyFireCb Callback)
{
	if(!Widget)	return;
	Widget->KeyFire = Callback;
}

void AxWin3_Widget_SetMouseMoveHandler(tAxWin3_Widget *Widget, tAxWin3_Widget_MouseMoveCb Callback)
{
	if(!Widget)	return;
	Widget->MouseMove = Callback;
}

void AxWin3_Widget_SetMouseButtonHandler(tAxWin3_Widget *Widget, tAxWin3_Widget_MouseBtnCb Callback)
{
	if(!Widget)	return;
	Widget->MouseButton = Callback;
}

// --- Manipulation
void AxWin3_Widget_SetFlags(tAxWin3_Widget *Widget, int FlagSet, int FlagMask)
{
	tWidgetIPC_SetFlags	msg;
	msg.WidgetID = Widget->ID;
	msg.Value = FlagSet;
	msg.Mask = FlagMask;
	
	AxWin3_SendIPC(Widget->Window, IPC_WIDGET_SETFLAGS, sizeof(msg), &msg);
}

void AxWin3_Widget_SetSize(tAxWin3_Widget *Widget, int Size)
{
	tWidgetIPC_SetSize	msg;
	
	msg.WidgetID = Widget->ID;
	msg.Value = Size;
	AxWin3_SendIPC(Widget->Window, IPC_WIDGET_SETSIZE, sizeof(msg), &msg);
}

void AxWin3_Widget_SetText(tAxWin3_Widget *Widget, const char *Text)
{
	char	buf[sizeof(tWidgetIPC_SetText) + strlen(Text) + 1];
	tWidgetIPC_SetText	*msg = (void*)buf;
	
	msg->WidgetID = Widget->ID;
	strcpy(msg->Text, Text);
	
	AxWin3_SendIPC(Widget->Window, IPC_WIDGET_SETTEXT, sizeof(buf), buf);
}

char *AxWin3_Widget_GetText(tAxWin3_Widget *Widget)
{
	char	buf[sizeof(tWidgetIPC_SetText)];
	tWidgetIPC_SetText	*msg = (void*)buf;
	size_t	retmsg_size;
	
	msg->WidgetID = Widget->ID;

	AxWin3_SendIPC(Widget->Window, IPC_WIDGET_GETTEXT, sizeof(buf), buf);

	msg = AxWin3_WaitIPCMessage(Widget->Window, IPC_WIDGET_GETTEXT, &retmsg_size);
	if( retmsg_size < sizeof(*msg) ) {
		free(msg);
		return NULL;
	}

	char	*ret = strndup(msg->Text, retmsg_size - sizeof(*msg));
	free(msg);
	return ret;
}

void AxWin3_Widget_SetColour(tAxWin3_Widget *Widget, int Index, tAxWin3_Colour Colour)
{
	tWidgetIPC_SetColour	msg;
	
	msg.WidgetID = Widget->ID;
	msg.Index = Index;
	msg.Colour = Colour;
	AxWin3_SendIPC(Widget->Window, IPC_WIDGET_SETCOLOUR, sizeof(msg), &msg);
}
