#ifndef __qwaq_app_h
#define __qwaq_app_h

#include <Object.h>

#include "event.h"
#include "qwaq-rect.h"

@class Group;
@class TextContext;

@interface QwaqApplication: Object
{
	qwaq_event_t event;
	qwaq_command endState;

	Group      *objects;

	TextContext *screen;
	Extent      screenSize;
	int         autocount;
}
-(Extent)size;
-(TextContext *)screen;
-run;
@end

@extern QwaqApplication *application;

#endif//__qwaq_app_h
