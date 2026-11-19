/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1's MIDI connection, shared by every instance of the plugin in the process (one device, one port).

   The device answers one SysEx request at a time and drops a second one sent before its reply
   (web/EDITOR_PROTOCOL.md, "One request at a time"), so every request goes through one queue here: the plugin's own
   (the parameters' automation, the dumps that follow the device) and those of each open editor (web/editor.html in
   the plugin's window, its WebMIDI replaced by this link: gui.cpp). A reply goes back to whoever asked; the device's
   pushes go to the instances and to the editors that watch. Notes, clock and transport are not requests: they go
   out at their time, between them.

   Threads: the audio thread talks to the link through each client's ring (lock-free); the editors from the UI
   thread (a mutex); the port's input arrives on the MIDI driver's thread; the link's own thread does the rest
   (step()). */
#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "ring.h"
#include "sysex.h"

namespace fm1 {

using Clock = std::chrono::steady_clock;
inline double nowSec() { return std::chrono::duration<double>(Clock::now().time_since_epoch()).count(); }

/* the MIDI port pair (RtMidi; a fake in the tests) */
class Port {
public:
    virtual ~Port() = default;
    virtual std::vector<std::string> inputs() = 0;
    virtual std::vector<std::string> outputs() = 0;
    /* opens the input and the output named so; incoming messages (whole, SysEx too) go to onIn; false: busy/gone */
    virtual bool open(const std::string &in, const std::string &out, std::function<void(const Bytes &)> onIn) = 0;
    virtual void close() = 0;
    virtual bool send(const Bytes &b) = 0;              /* false: the port failed (gone) */
    virtual std::string error() { return {}; }
};
std::unique_ptr<Port> makeRtMidiPort();

/* what the audio thread hands the link: a short MIDI message at a time, or a parameter's new value */
struct Ev {
    double when;                                        /* nowSec() it goes out at (0: at once) */
    uint8_t n, b[3];                                    /* n 1..3: a MIDI message; 0: a parameter */
    uint16_t p;                                         /* (a parameter) its firmware id */
    int16_t v;
};

/* an instance of the plugin, as the link sees it */
class Client {
public:
    virtual ~Client() = default;
    virtual int linkTrack() const = 0;                  /* 0..5 */
    virtual bool linkClock() const = 0;                 /* sends the clock (one instance does) */
    /* the device's values (the link's thread): one parameter, or the whole track (a dump: P_COUNT of them) */
    virtual void deviceParam(int p, int v) = 0;
    /* send: what the instance wants the device to take (a saved sound recalled), (p, value) */
    virtual void deviceDump(int engine, const std::vector<int16_t> &p, std::vector<std::pair<int, int>> &send) = 0;
    Ring<Ev, 4096> ring;                                /* the audio thread -> the link */
};

struct Status {
    bool connected = false, replying = false;
    std::string port, version, error;
    int selected = 0, ntrk = 6, pcount = 0;             /* pcount: the device's P_COUNT (0 not known yet) */
};

class Link {
public:
    static std::shared_ptr<Link> get();                 /* the process's one (made on first use, with RtMidi) */
    explicit Link(std::unique_ptr<Port> port, bool thread = true);
    ~Link();

    void addClient(Client *c);
    void removeClient(Client *c);
    void wantDump(int track);                           /* re-read a track (an instance moved to it) */

    /* the port: "" picks the first that looks like the FM-1 ("Felucca", "FM-1", ...) */
    void setPortName(const std::string &name);
    std::string portName();
    std::vector<std::string> portList();
    Status status();

    /* editors (the UI thread) */
    int editorOpen();
    void editorClose(int id);
    void editorSend(int id, const Bytes &b);            /* a message from the editor's WebMIDI output */
    std::vector<Bytes> editorTake(int id);              /* what came for it since */

    void step(double now);                              /* one pass of the link's work (its thread; the tests) */
    double requestTimeout = 0.35;                       /* s: a request without a reply is given up */

private:
    struct Req {
        int origin;                                     /* 0 the link's own, else the editor's id */
        Bytes frame;
        std::function<void(const Bytes &)> onReply;     /* (the link's own) */
        double deadline = 0;
    };
    struct Editor {
        int watch = 0;                                  /* what its WATCH asked for */
        std::vector<Bytes> in;
    };
    void run();
    void connect(double now);
    void disconnect(const std::string &why);
    void incoming(const Bytes &b);
    void handle(const Bytes &b, double now);
    void reply(const Bytes &b, double now);
    void push(const Bytes &b);
    void toEditors(const Bytes &b, int needWatch);
    void own(Bytes frame, std::function<void(const Bytes &)> onReply = {}, bool front = false);
    void sendNext(double now);
    void afterEditorReply(const Req &r, const Bytes &b);
    void dumpReply(const Bytes &b);
    void drainClients(double now);
    void sendDue(double now);
    void raw(const Bytes &b);

    std::unique_ptr<Port> port;
    std::mutex m;                                       /* everything below */
    std::vector<Client *> clients;
    std::map<int, Editor> editors;
    int nextEditor = 1;
    std::deque<Req> queue;                              /* waiting requests; editors' first */
    std::unique_ptr<Req> inFlight;
    std::deque<Bytes> rx;                               /* the port's input, for step() */
    std::multimap<double, Bytes> due;                   /* timed MIDI (notes, clock) */
    std::map<std::pair<int, int>, int> paramOut;        /* (track, p) -> the value to send */
    std::set<int> dumpWanted;
    Status st;
    std::string wantPort;
    bool portChanged = false;
    double nextScan = 0, lastReq = 0;
    int timeouts = 0;
    bool lastOwn = false;                               /* the last request the link's own (the next an editor's) */
    std::atomic<bool> quit{false};
    std::condition_variable cv;
    std::thread th;
};

}  // namespace fm1
