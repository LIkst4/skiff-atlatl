/* SPDX-License-Identifier: GPL-3.0-only */
#include "link.h"

#include <algorithm>
#include <regex>

#include "gen/params.h"

namespace fm1 {

namespace {
const std::regex UNIT_PORT("fm-1|smk|elite|mke-p37|starry|felucca|composite|sinco|usb-midi", std::regex::icase);
const std::regex FELUCCA("felucca", std::regex::icase);
constexpr int P_LEVEL = 0, P_PAN = 39, P_MUTE = 40;
}  // namespace

std::shared_ptr<Link> Link::get()
{
    static std::mutex gm;
    static std::weak_ptr<Link> one;
    std::lock_guard<std::mutex> g(gm);
    auto l = one.lock();
    if (!l) {
        l = std::make_shared<Link>(makeRtMidiPort());
        one = l;
    }
    return l;
}

Link::Link(std::unique_ptr<Port> p, bool thread) : port(std::move(p))
{
    if (thread)
        th = std::thread([this] { run(); });
}

Link::~Link()
{
    quit = true;
    cv.notify_all();
    if (th.joinable())
        th.join();
    std::lock_guard<std::mutex> g(m);
    if (st.connected)
        port->close();
}

void Link::run()
{
    while (!quit) {
        step(nowSec());
        std::unique_lock<std::mutex> g(m);
        cv.wait_for(g, std::chrono::microseconds(500));
    }
}

void Link::addClient(Client *c)
{
    std::lock_guard<std::mutex> g(m);
    clients.push_back(c);
    dumpWanted.insert(c->linkTrack());
}

void Link::removeClient(Client *c)
{
    std::lock_guard<std::mutex> g(m);
    clients.erase(std::remove(clients.begin(), clients.end(), c), clients.end());
}

void Link::wantDump(int track)
{
    std::lock_guard<std::mutex> g(m);
    dumpWanted.insert(track);
}

void Link::setPortName(const std::string &name)
{
    std::lock_guard<std::mutex> g(m);
    if (name == wantPort)
        return;
    wantPort = name;
    portChanged = true;
}

std::string Link::portName()
{
    std::lock_guard<std::mutex> g(m);
    return wantPort;
}

std::vector<std::string> Link::portList()
{
    std::lock_guard<std::mutex> g(m);
    return port->outputs();
}

Status Link::status()
{
    std::lock_guard<std::mutex> g(m);
    return st;
}

int Link::editorOpen()
{
    std::lock_guard<std::mutex> g(m);
    int id = nextEditor++;
    editors[id];
    return id;
}

void Link::editorClose(int id)
{
    std::lock_guard<std::mutex> g(m);
    editors.erase(id);
    queue.erase(std::remove_if(queue.begin(), queue.end(), [id](const Req &r) { return r.origin == id; }), queue.end());
}

void Link::editorSend(int id, const Bytes &b)
{
    std::lock_guard<std::mutex> g(m);
    auto e = editors.find(id);
    if (e == editors.end() || b.empty())
        return;
    if (!isOurs(b)) {                                   /* notes, the screen mirror's hello: not requests */
        if (st.connected)
            raw(b);
        return;
    }
    if (cmdOf(b) == WATCH && b.size() == 7) {           /* the link watches for everyone (3: TRACK_CHANGED too); */
        e->second.watch = b[5];                         /* the editor gets what it asked for, at once */
        e->second.in.push_back(frame(WATCH, {(uint8_t)b[5]}));
        return;
    }
    for (auto &r : queue)                               /* (its retry of a request still waiting here) */
        if (r.origin == id && r.frame == b)
            return;
    auto at = std::find_if(queue.begin(), queue.end(), [](const Req &r) { return r.origin == 0; });
    queue.insert(at, Req{id, b, {}, 0});                /* an editor's before the link's own */
    cv.notify_all();
}

std::vector<Bytes> Link::editorTake(int id)
{
    std::lock_guard<std::mutex> g(m);
    auto e = editors.find(id);
    if (e == editors.end())
        return {};
    std::vector<Bytes> v;
    v.swap(e->second.in);
    return v;
}

void Link::raw(const Bytes &b)
{
    if (!port->send(b))
        disconnect("the port failed");
}

void Link::own(Bytes f, std::function<void(const Bytes &)> onReply, bool front)
{
    Req r{0, std::move(f), std::move(onReply), 0};
    if (front)
        queue.push_front(std::move(r));
    else
        queue.push_back(std::move(r));
}

void Link::connect(double now)
{
    nextScan = now + 1.0;
    auto ins = port->inputs(), outs = port->outputs();
    auto pick = [&](const std::vector<std::string> &v, const std::string &want) -> std::string {
        if (!want.empty()) {
            for (auto &n : v)
                if (n == want)
                    return n;
            for (auto &n : v)
                if (n.find(want) != std::string::npos || want.find(n) != std::string::npos)
                    return n;
            return {};
        }
        for (auto &n : v)
            if (std::regex_search(n, FELUCCA))
                return n;
        for (auto &n : v)
            if (std::regex_search(n, UNIT_PORT))
                return n;
        return {};
    };
    std::string out = pick(outs, wantPort), in;
    if (out.empty()) {
        st.error = outs.empty() ? "no MIDI ports" : "no FM-1 port";
        return;
    }
    in = pick(ins, out);                                /* (the input of the same name) */
    if (in.empty())
        in = pick(ins, wantPort);
    if (in.empty()) {
        st.error = "no FM-1 input port";
        return;
    }
    if (!port->open(in, out, [this](const Bytes &b) {
            std::lock_guard<std::mutex> g(m);
            rx.push_back(b);
            cv.notify_all();
        })) {
        st.error = port->error().empty() ? "the port is busy (another program has it?)" : port->error();
        return;
    }
    st = Status{};
    st.connected = true;
    st.port = out;
    timeouts = 0;
    queue.erase(std::remove_if(queue.begin(), queue.end(), [](const Req &r) { return r.origin == 0; }), queue.end());
    inFlight.reset();
    own(frame(INFO), [this](const Bytes &b) {          /* version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, */
        size_t i = 5, n = b.size() - 1;                 /* NENGINES names, NTRK */
        std::string v;
        while (i < n && b[i])
            v += (char)b[i++];
        st.version = v;
        if (i + 6 > n)
            return;
        int neng = b[i + 1];
        i += 6;
        for (int k = 0; k < neng && i < n; k++) {
            while (i < n && b[i])
                i++;
            i++;
        }
        if (i < n && b[i] >= 1 && b[i] <= 8)
            st.ntrk = b[i];
    });
    own(frame(WATCH, {3}));
    own(frame(TRACK), [this](const Bytes &b) {
        if (b.size() > 7)
            st.selected = b[5];
    });
    for (auto *c : clients)
        dumpWanted.insert(c->linkTrack());
}

void Link::disconnect(const std::string &why)
{
    if (!st.connected)
        return;
    port->close();
    st.connected = false;
    st.replying = false;
    st.error = why;
    inFlight.reset();
    queue.clear();
    due.clear();                                        /* (the editors' requests: they time out, as unplugged) */
}

void Link::toEditors(const Bytes &b, int needWatch)
{
    for (auto &e : editors)
        if (e.second.watch & needWatch)
            e.second.in.push_back(b);
}

void Link::handle(const Bytes &b, double now)
{
    if (!isOurs(b)) {                                   /* the screen mirror's rows, the keys' frames: the editors' */
        if (!b.empty() && b[0] == 0xF0)
            for (auto &e : editors)
                e.second.in.push_back(b);
        return;
    }
    if (isPush(cmdOf(b)))
        push(b);
    else
        reply(b, now);
}

/* the device's pushes: to the instances (their parameters) and to the editors that watch */
void Link::push(const Bytes &b)
{
    uint8_t c = cmdOf(b);
    size_t n = b.size() - 6;                            /* the arguments' count */
    const uint8_t *a = b.data() + 5;
    if (c == CHANGED && n >= 4 && (a[0] == 0 || a[0] == 2)) {
        int p = a[1] + (a[0] == 2 ? 128 : 0), v = getV14(a[2], a[3]);
        for (auto *cl : clients)
            if (cl->linkTrack() == st.selected)
                cl->deviceParam(p, v);
    } else if (c == TRACK_CHANGED && n >= 4) {
        int t = a[0] & 63, p = a[1] + (a[0] & 0x40 ? 128 : 0), v = getV14(a[2], a[3]);
        for (auto *cl : clients)
            if (cl->linkTrack() == t)
                cl->deviceParam(p, v);
    } else if (c == RELOAD) {                           /* a load, another track selected: re-read */
        if (n >= 3)
            st.selected = a[2];
        dumpWanted.insert(st.selected);
    }
    toEditors(b, c == TRACK_CHANGED ? 2 : 1);
}

void Link::reply(const Bytes &b, double now)
{
    (void)now;
    if (!inFlight || cmdOf(inFlight->frame) != cmdOf(b))
        return;                                         /* (a late reply to a request given up) */
    std::unique_ptr<Req> r = std::move(inFlight);
    timeouts = 0;
    st.replying = true;
    if (r->origin == 0) {
        if (r->onReply)
            r->onReply(b);
    } else {
        auto e = editors.find(r->origin);
        if (e != editors.end())
            e->second.in.push_back(b);
        afterEditorReply(*r, b);
    }
}

/* what an editor's request changed: the instances follow it */
void Link::afterEditorReply(const Req &r, const Bytes &b)
{
    size_t n = b.size() - 6;
    const uint8_t *a = b.data() + 5;
    switch (cmdOf(r.frame)) {
    case SET:
        if (n >= 4 && (a[0] == 0 || a[0] == 2)) {
            for (auto *cl : clients)
                if (cl->linkTrack() == st.selected)
                    cl->deviceParam(a[1] + (a[0] == 2 ? 128 : 0), getV14(a[2], a[3]));
        } else if (n >= 4 && a[0] == 1 && a[1] == kGEngSel)
            dumpWanted.insert(st.selected);             /* (another engine: its defaults, its first preset) */
        break;
    case TRACK_PARAM:
        if (n >= 4)
            for (auto *cl : clients)
                if (cl->linkTrack() == (a[0] & 63))
                    cl->deviceParam(a[1] + (a[0] & 0x40 ? 128 : 0), getV14(a[2], a[3]));
        break;
    case TRACK_MIX:
        if (n >= 4)
            for (auto *cl : clients)
                if (cl->linkTrack() == a[0]) {
                    cl->deviceParam(P_LEVEL, getV14(a[1], a[2]));
                    cl->deviceParam(P_MUTE, a[3]);
                }
        break;
    case TRACK:
        if (n >= 2)
            st.selected = a[0];
        break;
    case PRESET:
    case UP_LOAD:
        dumpWanted.insert(st.selected);
        break;
    case PROJECT:
        if (r.frame.size() > 6 && r.frame[5] == 0)      /* (a load: every track) */
            for (auto *cl : clients)
                dumpWanted.insert(cl->linkTrack());
        break;
    default:
        break;
    }
}

void Link::dumpReply(const Bytes &b)
{
    size_t n = b.size() - 6;
    const uint8_t *a = b.data() + 5;
    if (n < 3 || (n - 3) % 2)
        return;
    int t = a[0], engine = a[1];
    std::vector<int16_t> p((n - 3) / 2);
    for (size_t i = 0; i < p.size(); i++)
        p[i] = (int16_t)getV14(a[3 + 2 * i], a[4 + 2 * i]);
    st.pcount = (int)p.size();
    for (auto *cl : clients)
        if (cl->linkTrack() == t) {
            std::vector<std::pair<int, int>> send;
            cl->deviceDump(engine, p, send);
            for (auto &pv : send)
                paramOut[{t, pv.first}] = pv.second;
        }
}

void Link::drainClients(double now)
{
    for (auto *c : clients) {
        Ev e;
        while (c->ring.pop(e)) {
            if (e.n) {
                if (st.connected)
                    due.emplace(e.when > 0 ? e.when : now, Bytes(e.b, e.b + e.n));
            } else
                paramOut[{c->linkTrack(), e.p}] = e.v;
        }
    }
}

void Link::sendDue(double now)
{
    while (!due.empty() && due.begin()->first <= now && st.connected) {
        raw(due.begin()->second);
        if (!due.empty())
            due.erase(due.begin());
    }
    if (due.size() > 8192)                              /* (a stuck clock: never more than this) */
        due.clear();
}

void Link::sendNext(double now)
{
    if (!st.connected || inFlight)
        return;
    bool editorWaits = !queue.empty() && queue.front().origin != 0;
    if (!paramOut.empty() && !(editorWaits && lastOwn)) {   /* the automation, alternating with the editors' */
        auto it = paramOut.begin();
        own(trackParamSet(it->first.first, it->first.second, it->second), [this, t = it->first.first,
                                                                            p = it->first.second](const Bytes &b) {
            size_t n = b.size() - 6;                    /* (the editors see it as a change on the device) */
            const uint8_t *a = b.data() + 5;
            if (n < 4)
                return;
            int v = getV14(a[2], a[3]);
            Bytes args;
            if (t == st.selected) {
                args = {(uint8_t)(p >= 128 ? 2 : 0), (uint8_t)(p & 127)};
                putV14(args, v);
                toEditors(frame(CHANGED, args), 1);
            } else if (p == P_LEVEL || p == P_PAN || p == P_MUTE) {
                args = {(uint8_t)t, (uint8_t)p};
                putV14(args, v);
                toEditors(frame(TRACK_CHANGED, args), 2);
            }
        }, true);
        paramOut.erase(it);
    } else if (queue.empty() && !dumpWanted.empty()) {
        int t = *dumpWanted.begin();
        dumpWanted.erase(dumpWanted.begin());
        own(frame(TRACK_DUMP, {(uint8_t)t}), [this](const Bytes &b) { dumpReply(b); });
    } else if (queue.empty() && now - lastReq > 1.0)
        own(frame(PING));                               /* (the watch's keep-alive) */
    if (queue.empty())
        return;
    inFlight = std::make_unique<Req>(std::move(queue.front()));
    queue.pop_front();
    lastOwn = inFlight->origin == 0;
    inFlight->deadline = now + requestTimeout;
    lastReq = now;
    raw(inFlight->frame);
}

void Link::step(double now)
{
    std::lock_guard<std::mutex> g(m);
    if (portChanged) {
        portChanged = false;
        disconnect("port changed");
        nextScan = 0;
    }
    if (!st.connected) {
        if (now >= nextScan)
            connect(now);
    } else if (now >= nextScan) {                       /* (unplugged: its name gone from the list) */
        nextScan = now + 1.0;
        auto outs = port->outputs();
        if (std::find(outs.begin(), outs.end(), st.port) == outs.end())
            disconnect("the FM-1 went away");
    }
    while (!rx.empty()) {
        Bytes b = std::move(rx.front());
        rx.pop_front();
        if (st.connected)
            handle(b, now);
    }
    drainClients(now);
    sendDue(now);
    if (inFlight && now > inFlight->deadline) {         /* no reply: given up (an editor retries by itself) */
        inFlight.reset();
        if (++timeouts >= 3)
            st.replying = false;
    }
    sendNext(now);
}

}  // namespace fm1
