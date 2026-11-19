/* SPDX-License-Identifier: GPL-3.0-only */
/* A clock's line: sample index -> the wall time it was seen at (nowSec), a least-squares line through the last points.
   Two clocks (the FM-1's USB audio, the DAW's blocks) each get one; the times they give are smooth (the callbacks'
   jitter averaged out) and follow the clocks' drift, so the FM-1's samples can be read at the DAW's rate (capture.cpp).
   A jump (a stall, a restart) starts the line over. */
#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

namespace fm1 {

class LineFit {
public:
    static constexpr int N = 1024;                    /* (4 s of 4 ms callbacks, 5 s of 256-sample blocks) */
    explicit LineFit(double nominalSecondsPerSample = 1.0 / 48000) : nominal(nominalSecondsPerSample) {}
    void reset(double secondsPerSample)
    {
        nominal = secondsPerSample;
        n = head = 0;
        ok = false;
    }
    /* the point: sample index x seen at time y */
    void add(double x, double y)
    {
        if (n && std::fabs(time(x) - y) > 0.25)          /* (a jump: the line over) */
            n = head = 0;
        xs[head] = x;
        ys[head] = y;
        head = (head + 1) % N;
        if (n < N)
            n++;
        double sx = 0, sy = 0;
        for (int i = 0; i < n; i++)
            sx += xs[i], sy += ys[i];
        mx = sx / n;
        my = sy / n;
        double sxx = 0, sxy = 0;
        for (int i = 0; i < n; i++) {
            double dx = xs[i] - mx;
            sxx += dx * dx;
            sxy += dx * (ys[i] - my);
        }
        slope = nominal;
        /* (enough of it: a slope from the points, within 1 % of the nominal one) */
        if (n >= 8 && sxx > 0 && std::sqrt(sxx / n) * nominal > 0.05) {
            double s = sxy / sxx;
            if (std::fabs(s / nominal - 1) < 0.01)
                slope = s;
        }
        ok = n >= 4;
    }
    bool ready() const { return ok; }
    double time(double x) const { return my + (x - mx) * slope; }
    double index(double t) const { return mx + (t - my) / slope; }
    double secondsPerSample() const { return slope; }

    /* a copy another thread reads (seqlock: the writer one thread, any readers) */
    struct Line {
        double mx, my, slope;
        bool ok;
        double time(double x) const { return my + (x - mx) * slope; }
        double index(double t) const { return mx + (t - my) / slope; }
    };
    Line line() const { return {mx, my, slope, ok}; }

private:
    double nominal;
    double xs[N], ys[N];
    int n = 0, head = 0;
    double mx = 0, my = 0, slope = 1.0 / 48000;
    bool ok = false;
};

/* LineFit::Line across threads: one writer, many readers */
class SharedLine {
    std::atomic<uint32_t> seq{0};
    std::atomic<double> mx{0}, my{0}, slope{1.0 / 48000};
    std::atomic<bool> ok{false};

public:
    void put(const LineFit::Line &l)
    {
        seq.fetch_add(1, std::memory_order_acq_rel);
        mx.store(l.mx, std::memory_order_relaxed);
        my.store(l.my, std::memory_order_relaxed);
        slope.store(l.slope, std::memory_order_relaxed);
        ok.store(l.ok, std::memory_order_relaxed);
        seq.fetch_add(1, std::memory_order_acq_rel);
    }
    LineFit::Line get() const
    {
        for (;;) {
            uint32_t a = seq.load(std::memory_order_acquire);
            LineFit::Line l{mx.load(std::memory_order_relaxed), my.load(std::memory_order_relaxed),
                            slope.load(std::memory_order_relaxed), ok.load(std::memory_order_relaxed)};
            std::atomic_thread_fence(std::memory_order_acquire);
            if (!(a & 1) && seq.load(std::memory_order_acquire) == a)
                return l;
        }
    }
};

}  // namespace fm1
