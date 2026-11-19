/* SPDX-License-Identifier: GPL-3.0-only */
#include "gui.h"

#include <cstring>
#include <optional>
#include <string>

#include "choc/gui/choc_MessageLoop.h"
#include "choc/gui/choc_WebView.h"
#include "plugin.h"
#include "resources.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#endif

namespace fm1 {

namespace {

#if defined(_WIN32)
constexpr const char *kApi = CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
constexpr const char *kApi = CLAP_WINDOW_API_COCOA;
#else
constexpr const char *kApi = CLAP_WINDOW_API_X11;
#endif

std::string mimeOf(const std::string &p)
{
    auto ends = [&](const char *e) { return p.size() >= std::strlen(e) && !p.compare(p.size() - std::strlen(e), std::string::npos, e); };
    return ends(".html")  ? "text/html"
           : ends(".js")  ? "text/javascript"
           : ends(".ttf") ? "font/ttf"
           : ends(".woff2") ? "font/woff2"
           : ends(".wasm") ? "application/wasm"
           : ends(".txt") ? "text/plain"
                          : "application/octet-stream";
}

#if defined(__linux__)
void gtkPump()
{
    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);
}
#endif

/* the JS array of byte arrays the page's __fm1In takes */
std::string jsFrames(const std::vector<Bytes> &v)
{
    std::string s = "window.__fm1In && window.__fm1In([";
    for (size_t i = 0; i < v.size(); i++) {
        s += i ? ",[" : "[";
        for (size_t k = 0; k < v[i].size(); k++) {
            if (k)
                s += ',';
            s += std::to_string(v[i][k]);
        }
        s += ']';
    }
    return s + "]);";
}

}  // namespace

Gui::Gui(Plugin &p, bool fl) : plugin(p), floating(fl)
{
#if defined(__linux__)
    static bool gtkUp = gtk_init_check(nullptr, nullptr);
    (void)gtkUp;
#endif
    choc::ui::WebView::Options o;
    o.acceptsFirstMouseClick = true;
    o.enableDefaultClipboardKeyShortcutsInSafari = true;
    o.fetchResource = [](const std::string &path) -> std::optional<choc::ui::WebView::Options::Resource> {
        std::string p = path == "/" || path.empty() ? "/editor.html" : path;
        auto q = p.find_first_of("?#");
        if (q != std::string::npos)
            p.resize(q);
        const Resource *r = findResource(p.substr(1));
        if (!r)
            return std::nullopt;
        return choc::ui::WebView::Options::Resource(std::string_view((const char *)r->data, r->size), mimeOf(p));
    };
    web = std::make_unique<choc::ui::WebView>(o);
    editorId = plugin.link ? plugin.link->editorOpen() : 0;
    web->bind("fm1Send", [this](const choc::value::ValueView &args) -> choc::value::Value {
        if (args.isArray() && args.size() && args[0].isArray() && plugin.link) {
            Bytes b;
            for (uint32_t i = 0; i < args[0].size(); i++)
                b.push_back((uint8_t)args[0][i].getWithDefault<int64_t>(0));
            plugin.link->editorSend(editorId, b);
        }
        return {};
    });
    web->bind("fm1Status", [this](const choc::value::ValueView &) -> choc::value::Value {
        return choc::value::createString(plugin.statusJson());
    });
    web->bind("fm1Set", [this](const choc::value::ValueView &args) -> choc::value::Value {
        if (args.isArray() && args.size() >= 2)
            plugin.setSetting(std::string(args[0].getWithDefault<std::string>("")), args[1].getWithDefault<std::string>(""));
        return {};
    });
    if (const Resource *shim = findResource("shim.js"))
        web->addInitScript(std::string((const char *)shim->data, shim->size));
    web->navigate({});                                  /* (the home page again: the shim before the editor's script) */
    if (!plugin.hostTimer || !plugin.hostTimer->register_timer(plugin.host, 16, &timerId))
        timerId = CLAP_INVALID_ID;
#if !defined(__linux__)
    if (timerId == CLAP_INVALID_ID)
        ownTimer = std::make_unique<choc::messageloop::Timer>(16, [this] {
            tick(CLAP_INVALID_ID);
            return true;
        });
#endif
#if defined(__linux__)
    if (floating) {
        GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(w), "FM-1 (skiff-atlatl)");
        gtk_window_set_default_size(GTK_WINDOW(w), (int)width, (int)height);
        gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(web->getViewHandle()));
        window = w;
    }
#endif
}

Gui::~Gui()
{
    ownTimer.reset();
    if (timerId != CLAP_INVALID_ID && plugin.hostTimer)
        plugin.hostTimer->unregister_timer(plugin.host, timerId);
    if (plugin.link)
        plugin.link->editorClose(editorId);
#if defined(__APPLE__)
    if (web)
        macDetach(web->getViewHandle());
#endif
    web.reset();
#if defined(__linux__)
    if (window)
        gtk_widget_destroy(GTK_WIDGET(window));
    gtkPump();
#endif
}

bool Gui::setParent(const clap_window_t *w)
{
    if (!w || !web)
        return false;
#if defined(_WIN32)
    HWND v = (HWND)web->getViewHandle();
    SetParent(v, (HWND)w->win32);
    LONG_PTR s = GetWindowLongPtr(v, GWL_STYLE);
    SetWindowLongPtr(v, GWL_STYLE, (s & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME)) | WS_CHILD | WS_VISIBLE);
    MoveWindow(v, 0, 0, (int)width, (int)height, TRUE);
    return true;
#elif defined(__APPLE__)
    macAttach(web->getViewHandle(), w->cocoa);
    macSetSize(web->getViewHandle(), width, height);
    return true;
#else
    GtkWidget *plug = gtk_plug_new((Window)w->x11);
    gtk_container_add(GTK_CONTAINER(plug), GTK_WIDGET(web->getViewHandle()));
    gtk_widget_set_size_request(plug, (int)width, (int)height);
    gtk_widget_show_all(plug);
    window = plug;
    gtkPump();
    return true;
#endif
}

bool Gui::setSize(uint32_t w, uint32_t h)
{
    width = w;
    height = h;
    if (!web)
        return false;
#if defined(_WIN32)
    MoveWindow((HWND)web->getViewHandle(), 0, 0, (int)w, (int)h, TRUE);
#elif defined(__APPLE__)
    macSetSize(web->getViewHandle(), w, h);
#else
    if (window) {
        if (floating)
            gtk_window_resize(GTK_WINDOW(window), (int)w, (int)h);
        else
            gtk_widget_set_size_request(GTK_WIDGET(window), (int)w, (int)h);
    }
    gtkPump();
#endif
    return true;
}

bool Gui::show()
{
#if defined(_WIN32)
    ShowWindow((HWND)web->getViewHandle(), SW_SHOW);
#elif defined(__linux__)
    if (window)
        gtk_widget_show_all(GTK_WIDGET(window));
    gtkPump();
#endif
    return true;
}

bool Gui::hide()
{
#if defined(_WIN32)
    ShowWindow((HWND)web->getViewHandle(), SW_HIDE);
#elif defined(__linux__)
    if (window)
        gtk_widget_hide(GTK_WIDGET(window));
    gtkPump();
#endif
    return true;
}

void Gui::setTitle(const char *t)
{
#if defined(__linux__)
    if (window && floating)
        gtk_window_set_title(GTK_WINDOW(window), t);
#else
    (void)t;
#endif
}

void Gui::tick(clap_id)
{
    if (plugin.link && web) {
        auto in = plugin.link->editorTake(editorId);
        for (size_t i = 0; i < in.size(); i += 64) {    /* (a few at a time: the dumps, the screen mirror) */
            std::vector<Bytes> part(in.begin() + i, in.begin() + std::min(in.size(), i + 64));
            web->evaluateJavascript(jsFrames(part));
        }
    }
#if defined(__linux__)
    gtkPump();
#endif
}

/* ---- the CLAP extension ---- */

namespace {

Plugin *self(const clap_plugin_t *p) { return static_cast<Plugin *>(p->plugin_data); }

bool isApi(const clap_plugin_t *, const char *api, bool floating)
{
    if (std::strcmp(api, kApi))
        return false;
#if defined(__linux__)
    (void)floating;
    return true;                                        /* (embedded through XEmbed, or a window of its own) */
#else
    return !floating;
#endif
}
bool preferredApi(const clap_plugin_t *, const char **api, bool *floating)
{
    *api = kApi;
#if defined(__linux__)
    *floating = true;                                   /* (WebKitGTK in a GtkPlug: hosts differ; a window always works) */
#else
    *floating = false;
#endif
    return true;
}
bool create(const clap_plugin_t *p, const char *api, bool floating)
{
    if (!isApi(p, api, floating) || self(p)->gui)
        return false;
    self(p)->gui = std::make_unique<Gui>(*self(p), floating);
    return true;
}
void destroy(const clap_plugin_t *p) { self(p)->gui.reset(); }
bool setScale(const clap_plugin_t *, double) { return false; }
bool getSize(const clap_plugin_t *p, uint32_t *w, uint32_t *h)
{
    if (!self(p)->gui)
        return false;
    *w = self(p)->gui->width;
    *h = self(p)->gui->height;
    return true;
}
bool canResize(const clap_plugin_t *) { return true; }
bool resizeHints(const clap_plugin_t *, clap_gui_resize_hints_t *h)
{
    h->can_resize_horizontally = true;
    h->can_resize_vertically = true;
    h->preserve_aspect_ratio = false;
    h->aspect_ratio_width = h->aspect_ratio_height = 1;
    return true;
}
bool adjustSize(const clap_plugin_t *, uint32_t *w, uint32_t *h)
{
    if (*w < 720)
        *w = 720;
    if (*h < 480)
        *h = 480;
    return true;
}
bool setSize(const clap_plugin_t *p, uint32_t w, uint32_t h) { return self(p)->gui && self(p)->gui->setSize(w, h); }
bool setParent(const clap_plugin_t *p, const clap_window_t *w) { return self(p)->gui && self(p)->gui->setParent(w); }
bool setTransient(const clap_plugin_t *, const clap_window_t *) { return false; }
void suggestTitle(const clap_plugin_t *p, const char *t)
{
    if (self(p)->gui)
        self(p)->gui->setTitle(t);
}
bool show(const clap_plugin_t *p) { return self(p)->gui && self(p)->gui->show(); }
bool hide(const clap_plugin_t *p) { return self(p)->gui && self(p)->gui->hide(); }

const clap_plugin_gui_t kGui = {isApi, preferredApi, create, destroy, setScale, getSize, canResize, resizeHints,
                                adjustSize, setSize, setParent, setTransient, suggestTitle, show, hide};

}  // namespace

const clap_plugin_gui_t *guiExtension() { return &kGui; }

}  // namespace fm1
