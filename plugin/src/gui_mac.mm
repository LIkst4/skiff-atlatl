/* SPDX-License-Identifier: GPL-3.0-only */
/* macOS: the webview's NSView into the host's. */
#import <Cocoa/Cocoa.h>

#include "gui.h"

namespace fm1 {

void macAttach(void *view, void *parent)
{
    NSView *v = (__bridge NSView *)view, *p = (__bridge NSView *)parent;
    [v removeFromSuperview];
    [p addSubview:v];
    [v setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
}

void macSetSize(void *view, uint32_t w, uint32_t h)
{
    NSView *v = (__bridge NSView *)view;
    [v setFrame:NSMakeRect(0, 0, w, h)];
}

void macDetach(void *view)
{
    NSView *v = (__bridge NSView *)view;
    [v removeFromSuperview];
}

}  // namespace fm1
