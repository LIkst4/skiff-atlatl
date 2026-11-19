/* SPDX-License-Identifier: GPL-3.0-only */
/* The link against a fake FM-1: one request in flight at most (the device drops a second), replies to whoever asked,
   the pushes to the instances and the watching editors, the automation coalesced, timed MIDI at its time, the port
   chosen and lost. */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "gen/params.h"
#include "link.h"

using namespace fm1;

static int bad = 0;
static void ck(const char *what, bool ok)
{
    std::printf("link: %-90s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* the FM-1 as the link sees it: answers one request at a time, after `delay`; a second request before the reply
   is dropped (and counted) */
struct FakeDevice : Port {
    std::vector<std::string> names{"Some Synth", "Felucca"};
    std::function<void(const Bytes &)> cb;
    bool opened = false, dropNext = false;
    int overlaps = 0, track = 0;
    double now = 0, delay = 0.01;
    std::vector<Bytes> sent;                            /* everything the link sent */
    std::vector<std::pair<double, Bytes>> pending;      /* replies to come */
    std::map<int, int> params;                          /* (track * 256 + p) */
    std::vector<int> mido;                              /* the times (ms) of non-SysEx messages */

    std::vector<std::string> inputs() override { return names; }
    std::vector<std::string> outputs() override { return names; }
    bool open(const std::string &in, const std::string &out, std::function<void(const Bytes &)> f) override
    {
        if (in != out)
            return false;
        cb = std::move(f);
        opened = true;
        return true;
    }
    void close() override { opened = false; }
    bool send(const Bytes &b) override
    {
        sent.push_back(b);
        if (!isOurs(b)) {
            mido.push_back((int)(now * 1000 + 0.5));
            return true;
        }
        if (!pending.empty()) {                         /* (the device: busy, the frame dropped whole) */
            overlaps++;
            return true;
        }
        if (dropNext) {
            dropNext = false;
            return true;
        }
        pending.emplace_back(now + delay, answer(b));
        return true;
    }
    Bytes answer(const Bytes &b)
    {
        uint8_t c = cmdOf(b);
        const uint8_t *a = b.data() + 5;
        switch (c) {
        case INFO:
            return frame(INFO, {'2', '.', '6', 0, 15, 127, 44, 64, 127, 0, 6, 16});
        case WATCH:
            return frame(WATCH, {a[0]});
        case TRACK:
            return frame(TRACK, {(uint8_t)track, 6, 0, 0, 0, 0, 0});
        case TRACK_DUMP: {
            Bytes r{a[0], 0, 0};
            for (uint32_t p = 0; p < kFirmwarePCount; p++)
                putV14(r, params.count(a[0] * 256 + p) ? params[a[0] * 256 + p] : 0);
            return frame(TRACK_DUMP, r);
        }
        case TRACK_PARAM: {
            int t = a[0] & 63, p = a[1] + (a[0] & 0x40 ? 128 : 0);
            if (b.size() > 8)
                params[t * 256 + p] = getV14(a[2], a[3]);
            Bytes r{a[0], a[1]};
            putV14(r, params[t * 256 + p]);
            return frame(TRACK_PARAM, r);
        }
        case GET: {
            Bytes r{a[0], a[1]};
            putV14(r, 42);
            return frame(GET, r);
        }
        default:
            return frame(c, {0});
        }
    }
    void tick(double t)
    {
        now = t;
        std::vector<Bytes> out;
        for (auto it = pending.begin(); it != pending.end();)
            if (it->first <= t) {
                out.push_back(it->second);
                it = pending.erase(it);
            } else
                ++it;
        for (auto &b : out)
            cb(b);
    }
    int count(uint8_t c) const
    {
        int n = 0;
        for (auto &b : sent)
            n += isOurs(b) && cmdOf(b) == c;
        return n;
    }
};

struct FakeClient : Client {
    int t = 0;
    bool clk = false;
    std::map<int, int> got;
    int dumps = 0;
    std::vector<std::pair<int, int>> recall;
    int linkTrack() const override { return t; }
    bool linkClock() const override { return clk; }
    void deviceParam(int p, int v) override { got[p] = v; }
    void deviceDump(int, const std::vector<int16_t> &p, std::vector<std::pair<int, int>> &send) override
    {
        dumps++;
        for (size_t i = 0; i < p.size(); i++)
            got[(int)i] = p[i];
        send = recall;
        recall.clear();
    }
};

int main()
{
    auto *dev = new FakeDevice;
    Link link(std::unique_ptr<Port>(dev), false);
    FakeClient c1, c2;
    c2.t = 2;
    link.addClient(&c1);
    link.addClient(&c2);
    double t = 0;
    auto run = [&](double until) {
        for (; t < until; t += 0.001) {
            dev->tick(t);
            link.step(t);
        }
    };
    run(0.5);
    Status s = link.status();
    ck("the FM-1's port chosen by its name (Felucca), opened, replying", dev->opened && s.connected && s.replying &&
                                                                         s.port == "Felucca");
    ck("on connecting: INFO (its version), WATCH 3, TRACK, a dump of each instance's track",
       dev->count(INFO) == 1 && s.version == "2.6" && dev->count(WATCH) == 1 && dev->count(TRACK) == 1 &&
           dev->count(TRACK_DUMP) == 2 && c1.dumps == 1 && c2.dumps == 1);
    ck("never a second request before the reply", dev->overlaps == 0);

    /* the automation: coalesced, a wide id's bit 7 in the track byte */
    Ev e{0, 0, {0, 0, 0}, 170, 33};
    c2.ring.push(e);
    e.v = 34;
    c2.ring.push(e);
    int before = dev->count(TRACK_PARAM);
    run(0.6);
    ck("an automated parameter: one TRACK_PARAM (the latest value), id 170 as track | 0x40",
       dev->count(TRACK_PARAM) == before + 1 && dev->params[2 * 256 + 170] == 34 &&
           dev->sent.back().size() >= 9);
    bool wide = false;
    for (auto &b : dev->sent)
        if (isOurs(b) && cmdOf(b) == TRACK_PARAM && b[5] == (2 | 0x40) && b[6] == 170 - 128)
            wide = true;
    ck("  (the frame as the protocol has it)", wide);

    /* an editor: its WATCH answered at once, its request's reply its own, the link's never */
    int ed = link.editorOpen();
    link.editorSend(ed, frame(WATCH, {1}));
    auto in = link.editorTake(ed);
    ck("an editor's WATCH answered by the link (the device watches already)", in.size() == 1 &&
                                                                             cmdOf(in[0]) == WATCH && in[0][5] == 1);
    int watches = dev->count(WATCH);
    Bytes g = frame(GET, {0, 5});
    link.editorSend(ed, g);
    link.editorSend(ed, g);                              /* (its retry, while the first still waits) */
    c1.ring.push(Ev{0, 0, {0, 0, 0}, 9, 77});
    run(0.8);
    in = link.editorTake(ed);
    int gets = 0, others = 0;
    for (auto &b : in)
        (cmdOf(b) == GET ? gets : others)++;
    ck("  its GET's reply to it (once: its retry not sent again), the link's TRACK_PARAM reply not",
       gets == 1 && dev->count(GET) == 1 && dev->count(WATCH) == watches);
    ck("  the link's automation of the selected track seen by the editor as a change (CHANGED)",
       others == 1 && cmdOf(in.back()) == CHANGED && in.back()[6] == 9);
    ck("  still one request at a time", dev->overlaps == 0);

    /* the device's pushes */
    Bytes ch{2, 170 - 128};
    putV14(ch, -5);
    dev->cb(frame(CHANGED, ch));                         /* (the selected track: T1) */
    Bytes tc{2, 0};
    putV14(tc, 99);
    dev->cb(frame(TRACK_CHANGED, tc));
    run(0.81);
    ck("CHANGED (scope 2: 128 + id) to the selected track's instance only", c1.got[170] == -5 && c2.got[170] != -5);
    ck("TRACK_CHANGED to that track's instance", c2.got[0] == 99);
    in = link.editorTake(ed);
    ck("  the editor (WATCH 1) gets CHANGED, not TRACK_CHANGED (it asked for bit 0 only)",
       in.size() == 1 && cmdOf(in[0]) == CHANGED);

    /* a recall: what the instance wants after the dump is sent */
    c2.recall = {{1, 11}, {171, 64}};
    link.wantDump(2);
    run(1.2);
    ck("a recall after the dump: the saved values sent", dev->params[2 * 256 + 1] == 11 && dev->params[2 * 256 + 171] == 64);

    /* timed MIDI: at its time, not before */
    c1.ring.push(Ev{t + 0.050, 3, {0x90, 60, 100}, 0, 0});
    c1.ring.push(Ev{t + 0.020, 1, {0xF8, 0, 0}, 0, 0});
    double t0 = t;
    dev->mido.clear();
    run(t + 0.1);
    ck("timed MIDI out at its time (clock 20 ms, note 50 ms; within 2 ms), in order",
       dev->mido.size() == 2 && std::abs(dev->mido[0] - (int)(t0 * 1000 + 20.5)) <= 2 &&
           std::abs(dev->mido[1] - (int)(t0 * 1000 + 50.5)) <= 2);

    /* a request without a reply: given up, the next goes */
    dev->dropNext = true;
    link.editorSend(ed, frame(GET, {0, 6}));
    c1.ring.push(Ev{0, 0, {0, 0, 0}, 7, 5});
    run(t + 1.8);
    ck("a request the device never answers is given up; the next one goes", dev->params[0 * 256 + 7] == 5);
    ck("  and the keep-alive PING goes when nothing else does", dev->count(PING) >= 1);

    /* the port goes */
    dev->names = {"Some Synth"};
    run(t + 1.5);
    s = link.status();
    ck("the FM-1 unplugged: disconnected, said so", !s.connected && !s.error.empty() && !dev->opened);
    dev->names = {"Some Synth", "Felucca"};
    run(t + 1.5);
    ck("  plugged back: connected again, the tracks read again", link.status().connected && c1.dumps >= 2);

    link.removeClient(&c1);
    link.removeClient(&c2);
    std::printf("%s\n", bad ? "LINK TESTS FAILED" : "link tests passed");
    return bad != 0;
}
