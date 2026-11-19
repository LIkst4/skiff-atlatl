/* SPDX-License-Identifier: GPL-3.0-only */
/* CAPTURE against a simulated FM-1 and DAW, each on its own drifting clock: the FM-1's USB audio at 44.1 kHz
   (+60 ppm) in 4 ms callbacks that arrive late and jittered, the DAW at 48 kHz (-40 ppm) in 256-sample blocks
   processed with jitter. Checks: the round trip measured as the plugin measures it (a note's onset), the note's sound
   landing L samples after the block that sent it (L the latency reported: the DAW's compensation puts it on the
   beat), still after two minutes of drift; the resampled tone clean; missing samples reported, never invented. */
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "capture.h"
#include "linefit.h"

using namespace fm1;

static int bad = 0;
static void ck(const char *what, bool ok)
{
    std::printf("capture: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

struct Sim {
    Capture cap;
    std::mt19937 rng{7};
    double fsDev = 44100 * (1 + 60e-6), fsHost = 48000 * (1 - 40e-6);
    double inLatency = 0.003, synthDelay = 0.005;       /* (USB in; MIDI out + the FM-1's own) */
    uint32_t perCb = 176, block = 256, chans = 8;
    uint64_t devK = 0;                                  /* the FM-1's samples made */
    double hostH = 0;
    LineFit hostFit{1.0 / 48000};
    double burstAt = -1;                                /* the production time of a note's sound on T2 (ch 3) */
    double nextCb, nextBlock;
    Sim()
    {
        cap.begin(44100, (int)chans);
        nextCb = perCb / fsDev + inLatency;
        nextBlock = 0;
    }
    float sample(uint32_t ch, uint64_t k)
    {
        double t = k / fsDev;
        if (ch == 2)
            return 0.5f * (float)std::sin(2 * M_PI * 997.0 * k / 44100.0);
        if (ch == 3)
            return burstAt >= 0 && t >= burstAt ? 0.3f : 0.0f;
        return 0;
    }
    void callback()
    {
        std::vector<float> buf((size_t)perCb * chans);
        for (uint32_t i = 0; i < perCb; i++)
            for (uint32_t c = 0; c < chans; c++)
                buf[(size_t)i * chans + c] = sample(c, devK + i);
        devK += perCb;
        std::uniform_real_distribution<double> j(0, 0.0015);
        cap.write(buf.data(), perCb, chans, nextCb);
        nextCb = devK / fsDev + perCb / fsDev + inLatency + j(rng);   /* (the next one's last sample, made, then late) */
    }
    /* run until the wall time t; onBlock(h, wall) for each DAW block */
    template <typename F> void run(double t, F onBlock)
    {
        std::uniform_real_distribution<double> j(0, 0.002);
        while (std::min(nextCb, nextBlock) < t) {
            if (nextCb <= nextBlock)
                callback();
            else {
                double now = nextBlock;
                hostFit.add(hostH, now);
                double wall = hostFit.ready() ? hostFit.time(hostH) : now;
                onBlock(hostH, wall);
                hostH += block;
                nextBlock = hostH / fsHost + j(rng);
            }
        }
    }
};

int main()
{
    Sim s;
    const uint32_t L = (uint32_t)std::ceil(0.020 * 48000);   /* the latency reported (20 ms) */
    s.run(3.0, [](double, double) {});
    ck("the FM-1's clock followed (3 s): its line ready", s.cap.latestTime() > 2.9);

    /* MEASURE: a note sent at a block's start, its onset found */
    double sent = 0, rt = 0, sentH = 0;
    s.run(3.5, [&](double h, double wall) {
        if (sent == 0) {
            sent = wall;
            sentH = h;
            s.burstAt = wall + s.synthDelay;            /* (the FM-1 plays it) */
        }
    });
    double got = s.cap.onset(3, sent - 0.002, sent + 0.4);
    rt = got - sent;
    ck("MEASURE: the round trip found (synth 5 ms + USB in 3 ms + the callbacks' lateness: 8..10 ms)",
       got > 0 && rt > 0.0078 && rt < 0.0105);

    /* the note's sound at the output: L samples after the block that sent it (read as the plugin reads: one reader,
       continuous; the note sent from one block's start) */
    CaptureReader rd;
    int chT2[1] = {3};
    auto landing = [&](double from, double until) {
        long first = -1;
        double sentHost = -1;
        std::vector<float> out(s.block);
        float *o[1] = {out.data()};
        s.burstAt = -1;
        s.run(from, [&](double, double wall) { rd.read(s.cap, chT2, o, 1, wall + rt - L / 48000.0, s.hostFit.secondsPerSample(), 48000, s.block); });
        s.run(until, [&](double h, double wall) {
            if (sentHost < 0) {
                sentHost = h;
                s.burstAt = wall + s.synthDelay;        /* (the FM-1 plays the note) */
            }
            rd.read(s.cap, chT2, o, 1, wall + rt - L / 48000.0, s.hostFit.secondsPerSample(), 48000, s.block);
            for (uint32_t i = 0; i < s.block && first < 0; i++)
                if (out[i] > 0.15f)
                    first = (long)(h + i);
        });
        return first < 0 ? 1e9 : first - sentHost;
    };
    double d = landing(4.0, 4.5);
    std::printf("capture:   (the sound at block + %.0f samples; L = %u)\n", d, L);
    ck("a note's sound lands L samples after its block (within 4 samples: 0.08 ms)", std::fabs(d - L) <= 4);

    /* two minutes of drift (100 ppm apart): still there */
    double d2 = landing(124.0, 124.5);
    std::printf("capture:   (after 2 minutes: block + %.0f samples)\n", d2);
    ck("after two minutes of the clocks drifting apart: still L samples after (within 4)", std::fabs(d2 - L) <= 4);

    /* the tone, resampled 44.1 -> 48 across the two clocks: clean */
    std::vector<float> tone;
    int chTone[1] = {2};
    CaptureReader rt2;
    s.run(125.5, [&](double, double wall) {
        std::vector<float> out(s.block);
        float *o[1] = {out.data()};
        bool all = rt2.read(s.cap, chTone, o, 1, wall + rt - L / 48000.0, s.hostFit.secondsPerSample(), 48000, s.block);
        if (all && wall > 125.0)                        /* (after 0.5 s: the reader settled) */
            tone.insert(tone.end(), out.begin(), out.end());
    });
    /* each 256-sample window, across a block's edge (from 128 on), fitted (a sin + b cos + c at the output's
       frequency): the error left (the interpolation, a step at the edge) and the window's phase (the slow wander of
       the two clocks' lines, as a time) */
    double f = 997.0 * s.fsDev / 44100.0 / s.fsHost, worst = 1e9, ph0 = 0, wander = 0, amp = 0;
    size_t n = tone.size(), wins = 0;
    for (size_t w0 = 128; w0 + 256 <= n && wins < 64; w0 += 256, wins++) {
        double A[3][3] = {}, B[3] = {};
        for (size_t i = w0; i < w0 + 256; i++) {
            double v[3] = {std::sin(2 * M_PI * f * i), std::cos(2 * M_PI * f * i), 1};
            for (int r = 0; r < 3; r++) {
                B[r] += v[r] * tone[i];
                for (int c = 0; c < 3; c++)
                    A[r][c] += v[r] * v[c];
            }
        }
        for (int r = 0; r < 3; r++)                     /* (Gauss) */
            for (int k = r + 1; k < 3; k++) {
                double m = A[k][r] / A[r][r];
                for (int c = 0; c < 3; c++)
                    A[k][c] -= m * A[r][c];
                B[k] -= m * B[r];
            }
        double x[3];
        for (int r = 2; r >= 0; r--) {
            x[r] = B[r];
            for (int c = r + 1; c < 3; c++)
                x[r] -= A[r][c] * x[c];
            x[r] /= A[r][r];
        }
        double sig = 0, res = 0;
        for (size_t i = w0; i < w0 + 256; i++) {
            double e = x[0] * std::sin(2 * M_PI * f * i) + x[1] * std::cos(2 * M_PI * f * i) + x[2];
            sig += e * e;
            res += (tone[i] - e) * (tone[i] - e);
        }
        worst = std::min(worst, 10 * std::log10(sig / (res + 1e-30)));
        double ph = std::atan2(x[1], x[0]);
        if (!wins)
            ph0 = ph;
        double dph = std::remainder(ph - ph0, 2 * M_PI);
        wander = std::max(wander, std::fabs(dph / (2 * M_PI * f) / 48000.0));
        amp = std::hypot(x[0], x[1]);
    }
    std::printf("capture:   (997 Hz through it: the worst window %.1f dB clean, amplitude %.3f; its timing wandered %.1f us)\n",
                worst, amp, wander * 1e6);
    ck("a 997 Hz tone through the two clocks: amplitude kept, every window 70 dB clean (no step at a block's edge)",
       wins == 64 && std::fabs(amp - 0.5) < 0.005 && worst > 70);
    ck("  its timing wanders 10 us at most (the lines' noise: a pitch change of a few ppm, inaudible)", wander < 10e-6);

    /* missing samples: said, silent */
    std::vector<float> out(64, 1.f);
    float *o1[1] = {out.data()};
    CaptureReader r3;
    bool okFuture = !r3.read(s.cap, chTone, o1, 1, s.cap.latestTime() + 0.05, 1 / 48000.0, 48000, 64);
    float mx = 0;
    for (float v : out)
        mx = std::max(mx, std::fabs(v));
    ck("samples not there yet: read says so and gives silence", okFuture && mx == 0);
    r3.reset();
    ck("samples gone (3 s ago, past the ring): the same",
       !r3.read(s.cap, chTone, o1, 1, s.cap.latestTime() - 3.5, 1 / 48000.0, 48000, 64));
    Capture two;
    two.begin(48000, 2);
    std::vector<float> st(2 * 480, 0.1f);
    two.write(st.data(), 480, 2, 1.0);
    two.write(st.data(), 480, 2, 1.01);
    CaptureReader r4;
    ck("an input of 2 channels: the tracks' channels silent, read says so",
       !r4.read(two, chT2, o1, 1, 1.005, 1 / 48000.0, 48000, 8) && out[0] == 0);
    std::printf("%s\n", bad ? "CAPTURE TESTS FAILED" : "capture tests passed");
    return bad != 0;
}
