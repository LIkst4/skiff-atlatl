/* SPDX-License-Identifier: GPL-3.0-only */
/* A minimal CLAP host for a look at the plugin's window (Linux, a floating window): loads the .clap, opens its GUI,
   runs its timer for a while, closes it. With a display (Xvfb in CI), a screenshot can be taken meanwhile.
     gui_smoke PLUGIN.clap SECONDS */
#include <clap/clap.h>
#include <dlfcn.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

static bool timerOn = false;
static bool regTimer(const clap_host_t *, uint32_t, clap_id *id)
{
    *id = 1;
    timerOn = true;
    return true;
}
static bool unregTimer(const clap_host_t *, clap_id) { return timerOn = false, true; }
static const clap_host_timer_support_t kTimer = {regTimer, unregTimer};
static void rescan(const clap_host_t *, clap_param_rescan_flags) {}
static void clearP(const clap_host_t *, clap_id, clap_param_clear_flags) {}
static void flushP(const clap_host_t *) {}
static const clap_host_params_t kParams = {rescan, clearP, flushP};
static const void *ext(const clap_host_t *, const char *id)
{
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT))
        return &kTimer;
    if (!std::strcmp(id, CLAP_EXT_PARAMS))
        return &kParams;
    return nullptr;
}
static void noop(const clap_host_t *) {}
static const clap_host_t kHost = {CLAP_VERSION_INIT, nullptr, "gui_smoke", "skiff-atlatl", "", "0", ext, noop, noop, noop};

int main(int argc, char **argv)
{
    if (argc < 3)
        return 2;
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        std::fprintf(stderr, "dlopen: %s\n", dlerror());
        return 1;
    }
    auto *entry = static_cast<const clap_plugin_entry_t *>(dlsym(lib, "clap_entry"));
    if (!entry || !entry->init(argv[1]))
        return 1;
    auto *f = static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin_t *p = f->create_plugin(f, &kHost, f->get_plugin_descriptor(f, 0)->id);
    if (!p || !p->init(p))
        return 1;
    auto *gui = static_cast<const clap_plugin_gui_t *>(p->get_extension(p, CLAP_EXT_GUI));
    auto *tm = static_cast<const clap_plugin_timer_support_t *>(p->get_extension(p, CLAP_EXT_TIMER_SUPPORT));
    if (!gui || !gui->create(p, CLAP_WINDOW_API_X11, true) || !gui->set_size(p, 1280, 840) || !gui->show(p)) {
        std::fprintf(stderr, "no window\n");
        return 1;
    }
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(std::atof(argv[2]) * 1000));
    while (std::chrono::steady_clock::now() < end) {
        if (timerOn && tm)
            tm->on_timer(p, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    gui->destroy(p);
    p->destroy(p);
    entry->deinit();
    std::printf("gui_smoke: the window opened, ran and closed\n");
    return 0;
}
