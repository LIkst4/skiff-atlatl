/* SPDX-License-Identifier: GPL-3.0-only */
/* CAPTURE: the FM-1's 8-channel USB audio (1-2 the master, 3-8 the tracks T1..T6, each before the shared effects),
   taken by the plugin itself (miniaudio: WASAPI, Core Audio, ALSA / PulseAudio), shared by every instance. Each
   instance reads its track (or the master) at the DAW's rate, its samples placed where the DAW's notes asked for them:
   the instance reports a fixed latency, the DAW compensates it, and the FM-1's sound lands on the beat.

   How: both clocks (the FM-1's samples, the DAW's blocks) are lines from sample index to wall time (linefit.h). A note
   sent at the DAW's sample h sounds back after the round trip R (MIDI out, the FM-1, USB audio in: measured,
   MEASURE); the instance's output sample h + L (L its latency) is the FM-1's sample that arrived at wall(h) + R. The
   two clocks' rates are followed sample by sample (cubic interpolation), so their drift never accumulates. */
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "linefit.h"

namespace fm1 {

struct CaptureStatus {
    bool running = false;
    std::string device, error;
    int channels = 0, rate = 0;
    double periodMs = 0;                                /* the callbacks' interval */
    uint64_t underruns = 0;
};

class Capture {
public:
    static std::shared_ptr<Capture> get();
    Capture();
    ~Capture();

    /* how many instances want it: the device is open while one does (the main thread) */
    void want(bool on);
    void setDeviceName(const std::string &name);        /* "" the first that looks like the FM-1 */
    std::string deviceName();
    std::vector<std::string> deviceList();              /* (cached a few seconds) */
    CaptureStatus status();

    /* the audio threads (any number of readers): n samples of the channels ch[0..nch) from the fractional sample
       index x0 on, dx apart; false and silence where the FM-1's samples are not there (not yet, or gone) */
    bool readAt(const int *ch, float *const *out, int nch, double x0, double dx, uint32_t n);
    LineFit::Line clock() const { return line.get(); }  /* sample index (+ 1) -> its arrival time */
    /* the first sample of channel ch (or the master's two: ch -1) louder than its floor between the times a and b:
       its arrival time; 0 none yet; -1 none (all of it there) */
    double onset(int ch, double a, double b);
    double latestTime();                                /* the arrival time of the newest sample (0: none) */

    /* the device's stream starts (capture.cpp open(); the tests) */
    void begin(int rate, int channels);
    double rate() const { return devRate.load(); }
    /* the device's callback (capture.cpp) */
    void write(const float *in, uint32_t frames, uint32_t channels, double now);

    static constexpr uint32_t RING = 1u << 17, CH = 8;  /* (2.7 s at 48 kHz) */
    void countUnderrun() { underruns.fetch_add(1, std::memory_order_relaxed); }

private:
    void open();
    void close();
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::mutex m;
    int users = 0;
    std::string wantDevice;
    CaptureStatus st;
    std::vector<float> ring;                            /* CH x RING */
    std::atomic<uint64_t> written{0};
    std::atomic<uint64_t> underruns{0};
    std::atomic<int> devChannels{0};
    std::atomic<double> devRate{48000};
    LineFit fit;                                        /* (the callback's) */
    SharedLine line;
    double lastCb = 0;
    std::atomic<double> period{0};
};

/* one reader's place in the FM-1's samples (an instance's): continuous from block to block, steered gently onto the
   arrival times asked for. The two clocks' lines move a little with every point; read straight off them each block,
   the samples would jump by that much at every block's edge (a 997 Hz tone: 11 dB clean), and their slopes are a few
   ppm off from block to block. Here the step is the nominal ratio, corrected by a loop (proportional: 0.2 % of the
   error each block; integral: the clocks' drift, learnt): the lines' noise becomes a wander of a few microseconds */
class CaptureReader {
public:
    /* n samples of the channels ch[0..nch) for the arrival times t0, t0 + dt, ... (dt: the DAW's sample, as its line
       has it; rate: the DAW's nominal one) */
    bool read(Capture &c, const int *ch, float *const *out, int nch, double t0, double dt, double rate, uint32_t n);
    void reset() { pos = -1; }

private:
    double pos = -1;                                    /* the next sample's fractional index, -1: none yet */
    double drift = 0;                                   /* the two clocks' ratio against the nominal one, learnt */
};

}  // namespace fm1
