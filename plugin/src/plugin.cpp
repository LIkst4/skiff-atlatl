/* SPDX-License-Identifier: GPL-3.0-only */
#include "plugin.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>

#include "gui.h"

namespace fm1 {

namespace {
constexpr const char *kId = "io.github.likst4.skiff-atlatl.fm1";
constexpr uint32_t kStateVersion = 1;
constexpr int F_ENUM = 8;

Plugin *self(const clap_plugin_t *p) { return static_cast<Plugin *>(p->plugin_data); }

/* the remote controls (Bitwig's): a page per group of the device's, eight at a time */
struct Page {
    std::string section, name;
    std::vector<clap_id> ids;
};
const std::vector<Page> &pages()
{
    static std::vector<Page> v = [] {
        std::vector<Page> out;
        std::map<std::string, size_t> at;
        for (uint32_t i = 0; i < kParamCount; i++) {
            std::string mod = kParams[i].module, sec = mod.substr(0, mod.find('/')), name = mod.substr(mod.find('/') + 1);
            auto f = at.find(mod);
            if (f == at.end() || out[f->second].ids.size() == 8) {
                at[mod] = out.size();
                out.push_back({sec, name, {}});
                if (f != at.end())
                    out.back().name += " 2";
            }
            out[at[mod]].ids.push_back(kParams[i].id);
        }
        return out;
    }();
    return v;
}
}  // namespace

Plugin::Plugin(const clap_host_t *h) : host(h)
{
    for (auto &x : idxOfP)
        x = -1;
    for (uint32_t i = 0; i < kParamCount; i++) {
        value[i] = kParams[i].def;
        dirty[i] = false;
        if (kParams[i].p < kFirmwarePCount)
            idxOfP[kParams[i].p] = (int)i;
    }
    plugin.desc = nullptr;
    plugin.plugin_data = this;
}

Plugin::~Plugin()
{
    if (link)
        link->removeClient(this);
}

bool Plugin::init()
{
    hostParams = static_cast<const clap_host_params_t *>(host->get_extension(host, CLAP_EXT_PARAMS));
    hostTimer = static_cast<const clap_host_timer_support_t *>(host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
    hostGui = static_cast<const clap_host_gui_t *>(host->get_extension(host, CLAP_EXT_GUI));
    link = Link::get();
    link->addClient(this);
    return true;
}

bool Plugin::activate(double sr)
{
    sampleRate = sr;
    playing = false;
    return true;
}

int Plugin::indexOfId(clap_id id) const
{
    for (uint32_t i = 0; i < kParamCount; i++)
        if (kParams[i].id == id)
            return (int)i;
    return -1;
}

/* ---- from the device ---- */

void Plugin::deviceParam(int p, int v)
{
    if (p < 0 || p >= (int)kFirmwarePCount || idxOfP[p] < 0)
        return;
    int i = idxOfP[p];
    if (value[i].load() == v)
        return;
    value[i] = (int16_t)v;
    dirty[i] = true;
    anyDirty = true;
    if (hostParams)
        hostParams->request_flush(host);
}

void Plugin::deviceDump(int eng, const std::vector<int16_t> &p, std::vector<std::pair<int, int>> &send)
{
    if (p.size() != kFirmwarePCount) {                  /* another firmware's layout: the parameters are not ours */
        mismatch = true;
        return;
    }
    mismatch = false;
    engine = eng;
    std::lock_guard<std::mutex> g(sm);
    bool recall = recallPending.exchange(false) && !saved.empty();
    recallBlocked = recall && savedEngine >= 0 && savedEngine != eng;
    if (recallBlocked)
        recall = false;
    for (uint32_t i = 0; i < kParamCount; i++) {
        int v = p[kParams[i].p];
        if (recall && saved[i] != v) {
            send.emplace_back(kParams[i].p, saved[i]);   /* (the saved sound, sent; the value stays the saved one) */
            continue;
        }
        if (value[i].load() != v) {
            value[i] = (int16_t)v;
            dirty[i] = true;
            anyDirty = true;
        }
    }
    if (recall || recallBlocked)
        saved.clear();
    if (anyDirty && hostParams)
        hostParams->request_flush(host);
}

/* ---- the audio thread ---- */

void Plugin::midi(double when, uint8_t a, uint8_t b, uint8_t c, uint8_t n)
{
    Ev e{when, n, {a, b, c}, 0, 0};
    ring.push(e);
}

void Plugin::inEvents(const clap_input_events_t *in, double now, double sr)
{
    uint32_t n = in->size(in);
    uint8_t ch = (uint8_t)(track.load() & 15);
    double off = offsetMs.load() / 1000.0;
    for (uint32_t k = 0; k < n; k++) {
        const clap_event_header_t *h = in->get(in, k);
        if (h->space_id != CLAP_CORE_EVENT_SPACE_ID)
            continue;
        double when = now + h->time / sr + off;
        switch (h->type) {
        case CLAP_EVENT_NOTE_ON:
        case CLAP_EVENT_NOTE_OFF: {
            auto *e = reinterpret_cast<const clap_event_note_t *>(h);
            if (!sendNotes || e->key < 0 || e->key > 127)
                break;
            int vel = (int)std::lround(e->velocity * 127.0);
            if (h->type == CLAP_EVENT_NOTE_ON)
                midi(when, 0x90 | ch, (uint8_t)e->key, (uint8_t)std::clamp(vel, 1, 127));
            else
                midi(when, 0x80 | ch, (uint8_t)e->key, (uint8_t)std::clamp(vel, 0, 127));
            break;
        }
        case CLAP_EVENT_MIDI: {                          /* (CCs, bends, pressure: on the track's channel) */
            auto *e = reinterpret_cast<const clap_event_midi_t *>(h);
            uint8_t s = e->data[0];
            if (!sendNotes || s < 0x80 || s >= 0xF0)
                break;
            uint8_t len = (s & 0xF0) == 0xC0 || (s & 0xF0) == 0xD0 ? 2 : 3;
            midi(when, (uint8_t)((s & 0xF0) | ch), e->data[1], e->data[2], len);
            break;
        }
        case CLAP_EVENT_PARAM_VALUE: {
            auto *e = reinterpret_cast<const clap_event_param_value_t *>(h);
            int i = indexOfId(e->param_id);
            if (i < 0)
                break;
            int v = (int)std::lround(std::clamp(e->value, (double)kParams[i].min, (double)kParams[i].max));
            if (value[i].load() == v)
                break;
            value[i] = (int16_t)v;
            Ev ev{0, 0, {0, 0, 0}, kParams[i].p, (int16_t)v};
            ring.push(ev);
            break;
        }
        default:
            break;
        }
    }
}

void Plugin::outParams(const clap_output_events_t *out)
{
    if (!anyDirty.exchange(false))
        return;
    for (uint32_t i = 0; i < kParamCount; i++)
        if (dirty[i].exchange(false)) {
            clap_event_param_value_t e{};
            e.header.size = sizeof e;
            e.header.time = 0;
            e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            e.header.type = CLAP_EVENT_PARAM_VALUE;
            e.header.flags = 0;
            e.param_id = kParams[i].id;
            e.cookie = nullptr;
            e.note_id = -1;
            e.port_index = -1;
            e.channel = -1;
            e.key = -1;
            e.value = value[i].load();
            out->try_push(out, &e.header);
        }
}

/* MIDI clock (24 a beat) and transport, from the DAW's: Start at the top, Song Position + Continue elsewhere (a
   loop's jump too), Stop */
void Plugin::clockBlock(const clap_process_t *p, double now)
{
    const clap_event_transport_t *t = p->transport;
    if (!sendClock || !t || !(t->flags & CLAP_TRANSPORT_HAS_TEMPO) || !(t->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE)) {
        if (playing && sendClock)
            midi(now, 0xFC, 0, 0, 1);
        playing = false;
        return;
    }
    double off = offsetMs.load() / 1000.0;
    bool play = (t->flags & CLAP_TRANSPORT_IS_PLAYING) != 0;
    double b0 = (double)t->song_pos_beats / (double)CLAP_BEATTIME_FACTOR, bps = t->tempo / 60.0 / sampleRate;
    double b1 = b0 + p->frames_count * bps;
    if (!play) {
        if (playing)
            midi(now + off, 0xFC, 0, 0, 1);
        playing = false;
        return;
    }
    if (!playing || std::fabs(b0 - expectBeat) > 0.01) {   /* a start, or the DAW's position jumped */
        if (playing)
            midi(now + off, 0xFC, 0, 0, 1);
        long six = std::max(0L, (long)std::floor(b0 * 4.0 + 1e-6));
        if (six == 0)
            midi(now + off, 0xFA, 0, 0, 1);
        else {
            midi(now + off, 0xF2, (uint8_t)(six & 127), (uint8_t)((six >> 7) & 127), 3);
            midi(now + off, 0xFB, 0, 0, 1);
        }
        nextTick = six / 4.0;
        while (nextTick < b0 - 1e-9)
            nextTick += 1.0 / 24.0;
        playing = true;
    }
    while (nextTick < b1) {
        double at = now + off + (nextTick - b0) / bps / sampleRate;
        midi(at, 0xF8, 0, 0, 1);
        nextTick += 1.0 / 24.0;
    }
    expectBeat = b1;
}

clap_process_status Plugin::process(const clap_process_t *p)
{
    double now = nowSec();
    inEvents(p->in_events, now, sampleRate);
    clockBlock(p, now);
    for (uint32_t o = 0; o < p->audio_outputs_count; o++) {
        auto &ob = p->audio_outputs[o];
        for (uint32_t c = 0; c < ob.channel_count; c++) {
            float *dst = ob.data32 ? ob.data32[c] : nullptr;
            if (!dst)
                continue;
            const float *src = nullptr;
            if (monitor && p->audio_inputs_count && p->audio_inputs[0].data32 && c < p->audio_inputs[0].channel_count)
                src = p->audio_inputs[0].data32[c];
            if (src)
                std::memmove(dst, src, sizeof(float) * p->frames_count);
            else
                std::memset(dst, 0, sizeof(float) * p->frames_count);
        }
    }
    outParams(p->out_events);
    return CLAP_PROCESS_CONTINUE;
}

void Plugin::paramsFlush(const clap_input_events_t *in, const clap_output_events_t *out)
{
    if (in)
        inEvents(in, nowSec(), sampleRate);
    if (out)
        outParams(out);
}

/* ---- parameters ---- */

bool Plugin::getParamInfo(uint32_t i, clap_param_info_t *info)
{
    if (i >= kParamCount)
        return false;
    const ParamDef &d = kParams[i];
    std::memset(info, 0, sizeof *info);
    info->id = d.id;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_IS_STEPPED | (d.fmt == F_ENUM && d.names ? CLAP_PARAM_IS_ENUM : 0);
    std::snprintf(info->name, sizeof info->name, "%s", d.name);
    std::snprintf(info->module, sizeof info->module, "%s", d.module);
    info->min_value = d.min;
    info->max_value = d.max;
    info->default_value = d.def;
    return true;
}

bool Plugin::getParamValue(clap_id id, double *v)
{
    int i = indexOfId(id);
    if (i < 0)
        return false;
    *v = value[i].load();
    return true;
}

bool Plugin::valueToText(clap_id id, double v, char *out, uint32_t cap)
{
    int i = indexOfId(id);
    if (i < 0)
        return false;
    const ParamDef &d = kParams[i];
    int n = (int)std::lround(v);
    if (d.fmt == F_ENUM && d.names && n >= d.min && n <= d.max)
        std::snprintf(out, cap, "%s", d.names[n - d.min]);
    else
        std::snprintf(out, cap, "%d", n);
    return true;
}

bool Plugin::textToValue(clap_id id, const char *text, double *v)
{
    int i = indexOfId(id);
    if (i < 0)
        return false;
    const ParamDef &d = kParams[i];
    if (d.fmt == F_ENUM && d.names)
        for (int k = 0; k <= d.max - d.min; k++)
            if (!std::strcmp(text, d.names[k])) {
                *v = d.min + k;
                return true;
            }
    char *end = nullptr;
    long n = std::strtol(text, &end, 10);
    if (end == text)
        return false;
    *v = std::clamp((double)n, (double)d.min, (double)d.max);
    return true;
}

/* ---- the state: the settings and the track's sound (recalled into the device once it is there) ---- */

bool Plugin::saveState(const clap_ostream_t *s)
{
    std::ostringstream o;
    Settings st = settings();
    o << "fm1-plugin " << kStateVersion << "\n"
      << "track " << st.track << "\nnotes " << st.notes << "\nclock " << st.clock << "\nmonitor " << st.monitor
      << "\noffset " << st.offsetMs << "\nport " << st.port << "\nengine " << engine.load() << "\n";
    for (uint32_t i = 0; i < kParamCount; i++)
        o << "p " << kParams[i].id << " " << value[i].load() << "\n";
    std::string b = o.str();
    const char *d = b.data();
    int64_t left = (int64_t)b.size();
    while (left > 0) {
        int64_t w = s->write(s, d, (uint64_t)left);
        if (w <= 0)
            return false;
        d += w;
        left -= w;
    }
    return true;
}

bool Plugin::loadState(const clap_istream_t *s)
{
    std::string b;
    char buf[4096];
    for (;;) {
        int64_t r = s->read(s, buf, sizeof buf);
        if (r < 0)
            return false;
        if (r == 0)
            break;
        b.append(buf, (size_t)r);
    }
    std::istringstream in(b);
    std::string key;
    uint32_t ver = 0;
    if (!(in >> key >> ver) || key != "fm1-plugin")
        return false;
    std::vector<int16_t> vals(kParamCount);
    for (uint32_t i = 0; i < kParamCount; i++)
        vals[i] = value[i].load();
    int eng = -1;
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        std::istringstream l(line);
        l >> key;
        if (key == "p") {
            long id, v;
            if (l >> id >> v) {
                int i = indexOfId((clap_id)id);
                if (i >= 0)
                    vals[i] = (int16_t)std::clamp(v, (long)kParams[i].min, (long)kParams[i].max);
            }
        } else if (key == "engine")
            l >> eng;
        else {
            std::string rest;
            std::getline(l >> std::ws, rest);
            setSetting(key, rest);
        }
    }
    for (uint32_t i = 0; i < kParamCount; i++)
        value[i] = vals[i];
    {
        std::lock_guard<std::mutex> g(sm);
        saved = vals;
        savedEngine = eng;
        recallPending = true;
        recallBlocked = false;
    }
    if (hostParams)
        hostParams->rescan(host, CLAP_PARAM_RESCAN_VALUES);
    if (link)
        link->wantDump(track.load());                   /* (the recall: once the device's values are known) */
    return true;
}

/* ---- the window's ---- */

Settings Plugin::settings()
{
    Settings s;
    s.track = track;
    s.notes = sendNotes;
    s.clock = sendClock;
    s.monitor = monitor;
    s.offsetMs = offsetMs;
    s.port = link ? link->portName() : std::string();
    return s;
}

void Plugin::setSetting(const std::string &k, const std::string &v)
{
    int n = std::atoi(v.c_str());
    if (k == "track" && n >= 0 && n < 6 && n != track.load()) {
        track = n;
        if (link)
            link->wantDump(n);
    } else if (k == "notes")
        sendNotes = n != 0;
    else if (k == "clock")
        sendClock = n != 0;
    else if (k == "monitor")
        monitor = n != 0;
    else if (k == "offset")
        offsetMs = std::clamp(n, -50, 50);
    else if (k == "port" && link)
        link->setPortName(v);
}

std::string Plugin::statusJson()
{
    Status s = link ? link->status() : Status{};
    Settings st = settings();
    auto q = [](const std::string &x) {
        std::string o = "\"";
        for (char c : x)
            o += c == '"' || c == '\\' ? std::string("\\") + c : (c >= 32 ? std::string(1, c) : std::string());
        return o + "\"";
    };
    std::ostringstream o;
    o << "{\"connected\":" << (s.connected ? "true" : "false") << ",\"replying\":" << (s.replying ? "true" : "false")
      << ",\"port\":" << q(s.port) << ",\"version\":" << q(s.version) << ",\"error\":" << q(s.error)
      << ",\"selected\":" << s.selected << ",\"mismatch\":" << (mismatch ? "true" : "false")
      << ",\"recallBlocked\":" << (recallBlocked ? "true" : "false") << ",\"track\":" << st.track
      << ",\"notes\":" << st.notes << ",\"clock\":" << st.clock << ",\"monitor\":" << st.monitor
      << ",\"offset\":" << st.offsetMs << ",\"wantPort\":" << q(st.port) << ",\"ports\":[";
    auto ports = link ? link->portList() : std::vector<std::string>{};
    for (size_t i = 0; i < ports.size(); i++)
        o << (i ? "," : "") << q(ports[i]);
    o << "]}";
    return o.str();
}

void Plugin::onTimer(clap_id id)
{
    if (gui)
        gui->tick(id);
}

/* ---- the C glue ---- */

namespace {

const char *kFeatures[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                           CLAP_PLUGIN_FEATURE_STEREO, nullptr};
const clap_plugin_descriptor_t kDesc = {
    CLAP_VERSION_INIT, kId, "FM-1 (skiff-atlatl)", "skiff-atlatl", "https://github.com/LIkst4/skiff-atlatl",
    "https://github.com/LIkst4/skiff-atlatl", "https://github.com/LIkst4/skiff-atlatl", FM1_PLUGIN_VERSION,
    "The FM-1 on skiff-atlatl firmware, in the DAW: its editor, its parameters as automation, notes, clock",
    kFeatures};

/* audio ports: the FM-1's sound back (MONITOR) in, stereo out */
uint32_t apCount(const clap_plugin_t *, bool) { return 1; }
bool apGet(const clap_plugin_t *, uint32_t i, bool input, clap_audio_port_info_t *info)
{
    if (i)
        return false;
    std::memset(info, 0, sizeof *info);
    info->id = input ? 1 : 0;
    std::snprintf(info->name, sizeof info->name, "%s", input ? "FM-1 in" : "Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}
const clap_plugin_audio_ports_t kAudioPorts = {apCount, apGet};

uint32_t npCount(const clap_plugin_t *, bool input) { return input ? 1 : 0; }
bool npGet(const clap_plugin_t *, uint32_t i, bool input, clap_note_port_info_t *info)
{
    if (i || !input)
        return false;
    std::memset(info, 0, sizeof *info);
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    std::snprintf(info->name, sizeof info->name, "Notes");
    return true;
}
const clap_plugin_note_ports_t kNotePorts = {npCount, npGet};

uint32_t prCount(const clap_plugin_t *) { return kParamCount; }
bool prInfo(const clap_plugin_t *p, uint32_t i, clap_param_info_t *info) { return self(p)->getParamInfo(i, info); }
bool prValue(const clap_plugin_t *p, clap_id id, double *v) { return self(p)->getParamValue(id, v); }
bool prToText(const clap_plugin_t *p, clap_id id, double v, char *o, uint32_t n) { return self(p)->valueToText(id, v, o, n); }
bool prFromText(const clap_plugin_t *p, clap_id id, const char *t, double *v) { return self(p)->textToValue(id, t, v); }
void prFlush(const clap_plugin_t *p, const clap_input_events_t *in, const clap_output_events_t *out)
{
    self(p)->paramsFlush(in, out);
}
const clap_plugin_params_t kParamsExt = {prCount, prInfo, prValue, prToText, prFromText, prFlush};

bool stSave(const clap_plugin_t *p, const clap_ostream_t *s) { return self(p)->saveState(s); }
bool stLoad(const clap_plugin_t *p, const clap_istream_t *s) { return self(p)->loadState(s); }
const clap_plugin_state_t kState = {stSave, stLoad};

uint32_t latGet(const clap_plugin_t *) { return 0; }
const clap_plugin_latency_t kLatency = {latGet};

void tmTick(const clap_plugin_t *p, clap_id id) { self(p)->onTimer(id); }
const clap_plugin_timer_support_t kTimer = {tmTick};

uint32_t rcCount(const clap_plugin_t *) { return (uint32_t)pages().size(); }
bool rcGet(const clap_plugin_t *, uint32_t i, clap_remote_controls_page_t *page)
{
    if (i >= pages().size())
        return false;
    const Page &pg = pages()[i];
    std::memset(page, 0, sizeof *page);
    std::snprintf(page->section_name, sizeof page->section_name, "%s", pg.section.c_str());
    std::snprintf(page->page_name, sizeof page->page_name, "%s", pg.name.c_str());
    page->page_id = i + 1;
    for (size_t k = 0; k < CLAP_REMOTE_CONTROLS_COUNT; k++)
        page->param_ids[k] = k < pg.ids.size() ? pg.ids[k] : CLAP_INVALID_ID;
    return true;
}
const clap_plugin_remote_controls_t kRemote = {rcCount, rcGet};

bool plInit(const clap_plugin_t *p) { return self(p)->init(); }
void plDestroy(const clap_plugin_t *p) { delete self(p); }
bool plActivate(const clap_plugin_t *p, double sr, uint32_t, uint32_t) { return self(p)->activate(sr); }
void plDeactivate(const clap_plugin_t *) {}
bool plStart(const clap_plugin_t *) { return true; }
void plStop(const clap_plugin_t *) {}
void plReset(const clap_plugin_t *) {}
clap_process_status plProcess(const clap_plugin_t *p, const clap_process_t *pr) { return self(p)->process(pr); }
const void *plExt(const clap_plugin_t *, const char *id)
{
    if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
        return &kAudioPorts;
    if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
        return &kNotePorts;
    if (!std::strcmp(id, CLAP_EXT_PARAMS))
        return &kParamsExt;
    if (!std::strcmp(id, CLAP_EXT_STATE))
        return &kState;
    if (!std::strcmp(id, CLAP_EXT_LATENCY))
        return &kLatency;
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT))
        return &kTimer;
    if (!std::strcmp(id, CLAP_EXT_REMOTE_CONTROLS) || !std::strcmp(id, CLAP_EXT_REMOTE_CONTROLS_COMPAT))
        return &kRemote;
    if (!std::strcmp(id, CLAP_EXT_GUI))
        return guiExtension();
    return nullptr;
}
void plMain(const clap_plugin_t *) {}

const clap_plugin_t *create(const clap_host_t *host)
{
    auto *p = new Plugin(host);
    p->plugin = {&kDesc, p, plInit, plDestroy, plActivate, plDeactivate, plStart, plStop, plReset, plProcess, plExt, plMain};
    return &p->plugin;
}

uint32_t fCount(const clap_plugin_factory_t *) { return 1; }
const clap_plugin_descriptor_t *fDesc(const clap_plugin_factory_t *, uint32_t i) { return i ? nullptr : &kDesc; }
const clap_plugin_t *fCreate(const clap_plugin_factory_t *, const clap_host_t *host, const char *id)
{
    if (!clap_version_is_compatible(host->clap_version) || std::strcmp(id, kId))
        return nullptr;
    return create(host);
}
const clap_plugin_factory_t kFactory = {fCount, fDesc, fCreate};

}  // namespace

}  // namespace fm1

extern "C" {
bool fm1_entry_init(const char *) { return true; }
void fm1_entry_deinit(void) {}
const void *fm1_entry_get_factory(const char *id)
{
    return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &fm1::kFactory : nullptr;
}
}
