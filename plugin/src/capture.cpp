/* SPDX-License-Identifier: GPL-3.0-only */
#include "capture.h"

#include <algorithm>
#include <cmath>
#include <regex>

#include "link.h"                                       /* (nowSec) */
#include "miniaudio.h"

namespace fm1 {

namespace {
const std::regex FM1_AUDIO("felucca|fm-1|smk|skiff", std::regex::icase);
}

struct Capture::Impl {
    ma_context ctx;
    bool ctxUp = false;
    ma_device dev;
    bool devUp = false;
    std::vector<std::string> names;
    double namesAt = -100;
};

std::shared_ptr<Capture> Capture::get()
{
    static std::mutex gm;
    static std::weak_ptr<Capture> one;
    std::lock_guard<std::mutex> g(gm);
    auto c = one.lock();
    if (!c) {
        c = std::make_shared<Capture>();
        one = c;
    }
    return c;
}

Capture::Capture() : impl(std::make_unique<Impl>()), ring((size_t)CH * RING, 0.f) {}

Capture::~Capture()
{
    std::lock_guard<std::mutex> g(m);
    close();
    if (impl->ctxUp)
        ma_context_uninit(&impl->ctx);
}

void Capture::want(bool on)
{
    std::lock_guard<std::mutex> g(m);
    users = std::max(0, users + (on ? 1 : -1));
    if (users && !st.running)
        open();
    else if (!users && st.running)
        close();
}

void Capture::setDeviceName(const std::string &name)
{
    std::lock_guard<std::mutex> g(m);
    if (name == wantDevice)
        return;
    wantDevice = name;
    if (st.running) {
        close();
        open();
    }
}

std::string Capture::deviceName()
{
    std::lock_guard<std::mutex> g(m);
    return wantDevice;
}

CaptureStatus Capture::status()
{
    std::lock_guard<std::mutex> g(m);
    if (users && !st.running && nowSec() - impl->namesAt > 2)
        open();                                         /* (not found before: plugged in since?) */
    CaptureStatus s = st;
    s.periodMs = period.load() * 1000;
    s.underruns = underruns.load();
    return s;
}

static void onData(ma_device *d, void *, const void *in, ma_uint32 frames)
{
    auto *c = static_cast<Capture *>(d->pUserData);
    if (in)
        c->write(static_cast<const float *>(in), frames, d->capture.channels, nowSec());
}

std::vector<std::string> Capture::deviceList()
{
    std::lock_guard<std::mutex> g(m);
    double now = nowSec();
    if (now - impl->namesAt < 3)
        return impl->names;
    impl->namesAt = now;
    impl->names.clear();
    if (!impl->ctxUp) {
        if (ma_context_init(nullptr, 0, nullptr, &impl->ctx) != MA_SUCCESS)
            return {};
        impl->ctxUp = true;
    }
    ma_device_info *play = nullptr, *cap = nullptr;
    ma_uint32 np = 0, nc = 0;
    if (ma_context_get_devices(&impl->ctx, &play, &np, &cap, &nc) == MA_SUCCESS)
        for (ma_uint32 i = 0; i < nc; i++)
            impl->names.emplace_back(cap[i].name);
    return impl->names;
}

void Capture::open()
{
    impl->namesAt = nowSec();
    if (!impl->ctxUp) {
        if (ma_context_init(nullptr, 0, nullptr, &impl->ctx) != MA_SUCCESS) {
            st.error = "no audio system";
            return;
        }
        impl->ctxUp = true;
    }
    ma_device_info *play = nullptr, *cap = nullptr;
    ma_uint32 np = 0, nc = 0;
    if (ma_context_get_devices(&impl->ctx, &play, &np, &cap, &nc) != MA_SUCCESS) {
        st.error = "the audio inputs can't be listed";
        return;
    }
    int pick = -1;
    for (ma_uint32 i = 0; i < nc && pick < 0; i++) {
        std::string n = cap[i].name;
        if (wantDevice.empty() ? std::regex_search(n, FM1_AUDIO) : n == wantDevice)
            pick = (int)i;
    }
    if (pick < 0) {
        st.error = wantDevice.empty() ? "no FM-1 audio input" : "the audio input is gone";
        return;
    }
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &cap[pick].id;
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 0;                           /* (the device's own: 8 with skiff-atlatl) */
    cfg.capture.shareMode = ma_share_mode_shared;
    cfg.sampleRate = 0;                                 /* (the device's own: 44.1 or 48 kHz) */
    cfg.performanceProfile = ma_performance_profile_low_latency;
    cfg.periodSizeInMilliseconds = 4;
    cfg.dataCallback = onData;
    cfg.pUserData = this;
    if (ma_device_init(&impl->ctx, &cfg, &impl->dev) != MA_SUCCESS) {
        st.error = "the FM-1's audio input can't be opened (another program has it?)";
        return;
    }
    impl->devUp = true;
    int ch = (int)impl->dev.capture.channels, rate = (int)impl->dev.sampleRate;
    begin(rate, ch);
    if (ma_device_start(&impl->dev) != MA_SUCCESS) {
        ma_device_uninit(&impl->dev);
        impl->devUp = false;
        st.error = "the FM-1's audio input won't start";
        return;
    }
    st = CaptureStatus{};
    st.running = true;
    st.device = cap[pick].name;
    st.channels = ch;
    st.rate = rate;
    if (ch < 8)
        st.error = std::to_string(ch) + " channels (the tracks need 8: the master only)";
}

void Capture::close()
{
    if (impl->devUp) {
        ma_device_uninit(&impl->dev);
        impl->devUp = false;
    }
    st.running = false;
    line.put(LineFit::Line{0, 0, 1.0 / 48000, false});
}

void Capture::begin(int rate, int channels)
{
    devChannels = channels;
    devRate = rate;
    fit.reset(1.0 / rate);
    line.put(LineFit::Line{0, 0, 1.0 / rate, false});
    lastCb = 0;
    period = 0;
}

void Capture::write(const float *in, uint32_t frames, uint32_t channels, double now)
{
    uint64_t w = written.load(std::memory_order_relaxed);
    uint32_t c = std::min<uint32_t>(channels, CH);
    for (uint32_t k = 0; k < c; k++) {
        float *r = &ring[(size_t)k * RING];
        for (uint32_t i = 0; i < frames; i++)
            r[(w + i) & (RING - 1)] = in[(size_t)i * channels + k];
    }
    w += frames;
    written.store(w, std::memory_order_release);
    if (lastCb > 0) {
        double p = now - lastCb, q = period.load();
        period = q > 0 ? q * 0.95 + p * 0.05 : p;
    }
    lastCb = now;
    fit.add((double)w, now);                            /* (the newest sample arrived now) */
    line.put(fit.line());
}

bool Capture::readAt(const int *ch, float *const *out, int nch, double x, double dx, uint32_t n)
{
    uint64_t w = written.load(std::memory_order_acquire);
    double lo = (double)w - (RING - 4096), hi = (double)w - 3;
    int chs = devChannels.load();
    bool all = true;
    for (uint32_t i = 0; i < n; i++, x += dx) {
        bool there = x >= lo && x <= hi && x >= 1;
        uint64_t k = there ? (uint64_t)x : 0;
        float f = (float)(x - (double)k);
        for (int j = 0; j < nch; j++) {
            if (!there || ch[j] < 0 || ch[j] >= chs || ch[j] >= (int)CH) {
                out[j][i] = 0;
                continue;
            }
            const float *r = &ring[(size_t)ch[j] * RING];
            float a = r[(k - 1) & (RING - 1)], b = r[k & (RING - 1)], c = r[(k + 1) & (RING - 1)],
                  d = r[(k + 2) & (RING - 1)];
            out[j][i] = b + 0.5f * f * (c - a + f * (2 * a - 5 * b + 4 * c - d + f * (3 * (b - c) + d - a)));   /* (Catmull-Rom) */
        }
        all &= there;
    }
    for (int j = 0; j < nch; j++)
        all &= ch[j] >= 0 && ch[j] < chs && ch[j] < (int)CH;
    if (!all)
        countUnderrun();
    return all;
}

double Capture::latestTime()
{
    LineFit::Line l = line.get();
    uint64_t w = written.load(std::memory_order_acquire);
    return l.ok ? l.time((double)w) : 0;
}

double Capture::onset(int ch, double a, double b)
{
    LineFit::Line l = line.get();
    uint64_t w = written.load(std::memory_order_acquire);
    int chs = devChannels.load();
    if (!l.ok || ch >= chs || ch >= (int)CH || (ch < 0 && chs < 2))
        return -1;
    auto at = [&](uint64_t k) -> float {
        if (ch >= 0)
            return ring[(size_t)ch * RING + (k & (RING - 1))];
        return 0.5f * (ring[k & (RING - 1)] + ring[RING + (k & (RING - 1))]);
    };
    double xa = l.index(a) - 1, xb = l.index(b) - 1, rate = 1.0 / l.slope;
    if (xa < (double)w - (RING - 8192) || xa < 1)
        return -1;
    float floor = 0;                                    /* (the 20 ms before: the noise, the sound before) */
    for (double x = std::max(1.0, xa - 0.02 * rate); x < xa && x < (double)w; x += 1)
        floor = std::max(floor, std::fabs(at((uint64_t)x)));
    float thr = std::max(0.003f, floor * 4);
    uint64_t end = (uint64_t)std::min(xb, (double)w);
    for (uint64_t k = (uint64_t)std::max(1.0, xa); k < end; k++)
        if (std::fabs(at(k)) > thr)
            return l.time((double)k + 1);
    return xb < (double)w ? -1 : 0;
}

bool CaptureReader::read(Capture &c, const int *ch, float *const *out, int nch, double t0, double dt, double rate,
                         uint32_t n)
{
    LineFit::Line l = c.clock();
    if (!l.ok || n == 0) {
        for (int j = 0; j < nch; j++)
            std::fill(out[j], out[j] + n, 0.f);
        pos = -1;
        return false;
    }
    double target = l.index(t0) - 1;                    /* (the k-th sample arrived with the k + 1-th point) */
    double nominal = c.rate() / rate;
    if (pos < 0 || std::fabs(target - pos) > 0.01 / l.slope) {   /* (10 ms off: a start, a jump; there at once) */
        pos = target;
        drift = dt / l.slope / nominal - 1;            /* (from the lines, to start with) */
    }
    double e = target - pos;
    drift += 1e-6 * e / (nominal * n);                  /* (integral: p^2 / 4, critically damped) */
    drift = std::clamp(drift, -0.002, 0.002);
    double step = nominal * (1 + drift) + 0.002 * e / n;
    bool all = c.readAt(ch, out, nch, pos, step, n);
    pos += step * n;
    return all;
}

}  // namespace fm1
