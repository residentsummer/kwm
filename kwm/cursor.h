#ifndef CURSOR_H
#define CURSOR_H

#include "axlib/window.h"
#include <Carbon/Carbon.h>

void FocusWindowBelowCursor();
void MoveCursorToFocusedWindow();
void MoveCursorToWindow(ax_window *Window);
void MoveCursorToFocusedWindow(bool Recenter);
void MoveCursorToWindow(ax_window *Window, bool Recenter);

#endif
