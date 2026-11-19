/* SPDX-License-Identifier: GPL-3.0-only */
/* The plugin: one instance per FM-1 track. Its parameters are the track's (automatable, two ways: the DAW's moves go
   to the device, the device's knobs come back as changes the DAW can record); the notes it gets go out on the
   track's MIDI channel; one instance can send the MIDI clock and the transport. DIRECT: the FM-1's audio comes back
   on the DAW's own audio input (or through this plugin's input, MONITOR), no latency added. */
#pragma once
#include <clap/clap.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "gen/params.h"
#include "link.h"

namespace fm1 {

class Gui;

struct Settings {
    int track = 0;                                      /* 0..5: T1..T6 (MIDI channel track + 1) */
    bool notes = true, clock = false, monitor = true;
    int offsetMs = 0;                                   /* the MIDI's timing against the DAW's (-50..50) */
    std::string port;                                   /* "" the first FM-1 found */
};

class Plugin : public Client {
public:
    Plugin(const clap_host_t *host);
    ~Plugin() override;
    clap_plugin_t plugin;

    /* Client */
    int linkTrack() const override { return track.load(); }
    bool linkClock() const override { return sendClock.load(); }
    void deviceParam(int p, int v) override;
    void deviceDump(int engine, const std::vector<int16_t> &p, std::vector<std::pair<int, int>> &send) override;

    /* the window's (main thread) */
    Settings settings();
    void setSetting(const std::string &key, const std::string &value);
    std::string statusJson();
    std::shared_ptr<Link> link;
    const clap_host_t *host;

    /* clap */
    bool init();
    bool activate(double sr);
    clap_process_status process(const clap_process_t *p);
    void paramsFlush(const clap_input_events_t *in, const clap_output_events_t *out);
    bool getParamInfo(uint32_t i, clap_param_info_t *info);
    bool getParamValue(clap_id id, double *v);
    bool valueToText(clap_id id, double v, char *out, uint32_t cap);
    bool textToValue(clap_id id, const char *text, double *v);
    bool saveState(const clap_ostream_t *s);
    bool loadState(const clap_istream_t *s);
    void onTimer(clap_id id);

    std::unique_ptr<Gui> gui;
    const clap_host_params_t *hostParams = nullptr;
    const clap_host_timer_support_t *hostTimer = nullptr;
    const clap_host_gui_t *hostGui = nullptr;

private:
    int indexOfId(clap_id id) const;
    void inEvents(const clap_input_events_t *in, double now, double sr);
    void outParams(const clap_output_events_t *out);
    void midi(double when, uint8_t a, uint8_t b, uint8_t c, uint8_t n = 3);
    void clockBlock(const clap_process_t *p, double now);

    std::atomic<int> track{0}, offsetMs{0};
    std::atomic<bool> sendNotes{true}, sendClock{false}, monitor{true};
    std::atomic<int16_t> value[kParamCount];
    std::atomic<bool> dirty[kParamCount];
    std::atomic<bool> anyDirty{false};
    int idxOfP[kFirmwarePCount];
    double sampleRate = 48000;
    std::mutex sm;                                      /* the recall below (the main thread, the link's) */
    std::vector<int16_t> saved;                         /* a loaded state's values (by index), until the device has them */
    int savedEngine = -1;
    std::atomic<int> engine{-1};
    std::atomic<bool> recallPending{false}, recallBlocked{false}, mismatch{false};
    /* the clock (the audio thread) */
    bool playing = false;
    double nextTick = 0, expectBeat = 0;
};

}  // namespace fm1
