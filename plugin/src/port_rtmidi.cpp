/* SPDX-License-Identifier: GPL-3.0-only */
/* The link's port: RtMidi (CoreMIDI, WinMM, ALSA). */
#include <RtMidi.h>

#include "link.h"

namespace fm1 {

namespace {

class RtPort : public Port {
    std::unique_ptr<RtMidiIn> in;
    std::unique_ptr<RtMidiOut> out;
    std::unique_ptr<RtMidiIn> probeIn;                  /* (listing only) */
    std::unique_ptr<RtMidiOut> probeOut;
    std::function<void(const Bytes &)> cb;
    std::string err;

    static void onMessage(double, std::vector<unsigned char> *msg, void *self)
    {
        auto *p = static_cast<RtPort *>(self);
        if (msg && !msg->empty() && p->cb)
            p->cb(Bytes(msg->begin(), msg->end()));
    }
    template <typename T> static std::vector<std::string> names(std::unique_ptr<T> &dev)
    {
        std::vector<std::string> v;
        try {
            if (!dev)
                dev = std::make_unique<T>(RtMidi::UNSPECIFIED, "skiff-atlatl");
            unsigned n = dev->getPortCount();
            for (unsigned i = 0; i < n; i++)
                v.push_back(dev->getPortName(i));
        } catch (const RtMidiError &) {
        }
        return v;
    }
    template <typename T> static int indexOf(T &dev, const std::string &name)
    {
        unsigned n = dev.getPortCount();
        for (unsigned i = 0; i < n; i++)
            if (dev.getPortName(i) == name)
                return (int)i;
        return -1;
    }

public:
    std::vector<std::string> inputs() override { return names(probeIn); }
    std::vector<std::string> outputs() override { return names(probeOut); }

    bool open(const std::string &inName, const std::string &outName, std::function<void(const Bytes &)> onIn) override
    {
        close();
        err.clear();
        try {
            in = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "skiff-atlatl", 1024);
            out = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, "skiff-atlatl");
            in->setBufferSize(4096, 16);                /* (WinMM: the dumps, the screen mirror's rows) */
            int i = indexOf(*in, inName), o = indexOf(*out, outName);
            if (i < 0 || o < 0) {
                err = "the FM-1's port is gone";
                close();
                return false;
            }
            cb = std::move(onIn);
            in->ignoreTypes(false, true, true);         /* SysEx in; the device's clock and sensing not */
            in->setCallback(&RtPort::onMessage, this);
            in->openPort((unsigned)i, "skiff-atlatl in");
            out->openPort((unsigned)o, "skiff-atlatl out");
            return true;
        } catch (const RtMidiError &e) {
            err = e.getMessage();
            close();
            return false;
        }
    }
    void close() override
    {
        try {
            if (in) {
                in->cancelCallback();
                in->closePort();
            }
            if (out)
                out->closePort();
        } catch (const RtMidiError &) {
        }
        in.reset();
        out.reset();
    }
    bool send(const Bytes &b) override
    {
        if (!out)
            return false;
        try {
            out->sendMessage(b.data(), b.size());
            return true;
        } catch (const RtMidiError &e) {
            err = e.getMessage();
            return false;
        }
    }
    std::string error() override { return err; }
    ~RtPort() override { close(); }
};

}  // namespace

std::unique_ptr<Port> makeRtMidiPort() { return std::make_unique<RtPort>(); }

}  // namespace fm1
