#ifndef HANDLEWINDOW_H
#define HANDLEWINDOW_H

#include <Bitmap.h>
#include <Messenger.h>
#include <View.h>
#include <Window.h>

enum
{
	M_FLOATER_ACTION = 'flac'
};


enum
{
	FLOATER_NONE = 0,
	FLOATER_RESIZE,
	FLOATER_MOVE
};

// The Floater class is a generic interface for attaching floating manipulators
// to a view to provide direct editing.
class Floater
{
public:
						Floater(void);
						Floater(const int32 &action);
	virtual				~Floater(void);
	
	virtual	void		FloaterAttached(BView *handler);
	virtual	void		FloaterDetached(BView *handler);
	
	virtual	void		UpdateFloaterPosition(const BRect &rect, const int32 &action);
	
	virtual	void		SetAction(const int32 &action);
			int32		GetAction(void) const;

private:
	int32				fAction;
};


class HandleView;

// The HandleFloater class is for floaters which are meant to be dragged, like
// move and resize operations.
class HandleFloater : public BWindow, public Floater
{
public:
						HandleFloater(const char *bitmapName, const int32 &action);
						~HandleFloater(void);
						
			void		SetBitmap(BBitmap *bitmap);
			BBitmap *	GetBitmap(void);

			void		FloaterAttached(BView *handler);
			void		FloaterDetached(BView *handler);
			
	virtual	void		UpdateFloaterPosition(const BRect &rect, const int32 &action);
	
private:
			HandleView	*fView;
};


class HandleView : public BView
{
public:
						HandleView(BRect frame, const int32 &action);
	virtual				~HandleView(void);
	virtual	void		Draw(BRect rect);
	
	virtual	void		MouseDown(BPoint pt);
	
			void		SetMessenger(BMessenger &msgr) { fTarget = msgr; }
			void		SendMessage(BMessage &msg) { fTarget.SendMessage(&msg); }
private:
	friend class HandleFloater;
	
	BBitmap		*fBitmap;
	BMessenger	fTarget;
	int32		fAction;
};


#endif
