/* SPDX-License-Identifier: GPL-3.0-only */
/* The plugin's window: web/editor.html in a webview (choc: WebView2, WKWebView, WebKitGTK), its WebMIDI the link
   (shim.js), and this instance's settings on a bar above it. */
#pragma once
#include <clap/clap.h>

#include <memory>

namespace choc::ui {
class WebView;
}
namespace choc::messageloop {
struct Timer;
}

namespace fm1 {

class Plugin;

class Gui {
public:
    explicit Gui(Plugin &p, bool floating);
    ~Gui();
    bool setParent(const clap_window_t *w);
    bool setSize(uint32_t w, uint32_t h);
    bool show();
    bool hide();
    void setTitle(const char *t);
    void tick(clap_id id);                              /* the host's timer: the link's input to the page */
    uint32_t width = 1280, height = 840;

private:
    void pump();
    Plugin &plugin;
    bool floating;
    int editorId = 0;
    clap_id timerId = CLAP_INVALID_ID;
    std::unique_ptr<choc::ui::WebView> web;
    void *window = nullptr;                             /* (Linux: the GtkPlug or the floating GtkWindow) */
    std::unique_ptr<choc::messageloop::Timer> ownTimer; /* (no host timer: a main-thread one of our own) */
};

const clap_plugin_gui_t *guiExtension();

#if defined(__APPLE__)
void macAttach(void *view, void *parent);
void macSetSize(void *view, uint32_t w, uint32_t h);
void macDetach(void *view);
#endif

}  // namespace fm1
