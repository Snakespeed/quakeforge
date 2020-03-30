#ifndef __qwaq_debugger_debugger_h
#define __qwaq_debugger_debugger_h

#include <types.h>
#include <Object.h>

#include "debugger/debug.h"
#include "debugger/localsview.h"

@class ProxyView;
@class Editor;
@class ScrollBar;
@class Window;
@class Array;

@interface Debugger : Object
{
	qdb_target_t target;

	Window     *source_window;
	ProxyView  *file_proxy;
	Array      *files;
	Editor     *current_file;

	Window     *locals_window;
	ScrollBar  *scrollbar;
	LocalsView *locals_view;
}
+(Debugger *)withTarget:(qdb_target_t)target;
-initWithTarget:(qdb_target_t) target;
-(qdb_target_t)target;
-handleDebugEvent;
@end

#endif//__qwaq_debugger_debugger_h
