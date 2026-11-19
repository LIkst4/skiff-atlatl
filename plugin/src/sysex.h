/* SPDX-License-Identifier: GPL-3.0-only */
/* The FM-1 editor protocol's framing (web/EDITOR_PROTOCOL.md): F0 7D 46 4C <cmd> <args> F7, values as v14. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fm1 {

enum Cmd : uint8_t {
    INFO = 1, GET = 2, SET = 3, DUMP = 4, PRESET = 8, PROJECT = 9, UP_LOAD = 20, WATCH = 22, CHANGED = 23,
    RELOAD = 24, PING = 25, STEP_CHANGED = 26, TRACK = 27, TRACK_MIX = 28, TRACK_DUMP = 29, TRACK_PARAM = 31,
    TRACK_CHANGED = 32
};

using Bytes = std::vector<uint8_t>;

inline bool isOurs(const uint8_t *d, size_t n)
{
    return n >= 6 && d[0] == 0xF0 && d[1] == 0x7D && d[2] == 0x46 && d[3] == 0x4C && d[n - 1] == 0xF7;
}
inline bool isOurs(const Bytes &b) { return isOurs(b.data(), b.size()); }
/* the command of one of our frames; its arguments are b[5 .. size - 2] */
inline uint8_t cmdOf(const Bytes &b) { return b[4]; }
inline bool isPush(uint8_t c) { return c == CHANGED || c == RELOAD || c == STEP_CHANGED || c == TRACK_CHANGED; }

inline void putV14(Bytes &b, int v)
{
    v += 8192;
    b.push_back((uint8_t)(v & 127));
    b.push_back((uint8_t)((v >> 7) & 127));
}
inline int getV14(uint8_t lo, uint8_t hi) { return (int)(lo | (hi << 7)) - 8192; }

inline Bytes frame(uint8_t cmd, const Bytes &args = {})
{
    Bytes b{0xF0, 0x7D, 0x46, 0x4C, cmd};
    b.insert(b.end(), args.begin(), args.end());
    b.push_back(0xF7);
    return b;
}
/* TRACK_PARAM's set; an id from 128 on: bit 6 of the track byte its bit 7 ("Wide ids") */
inline Bytes trackParamSet(int track, int id, int v)
{
    Bytes a{(uint8_t)(track | (id & 128 ? 0x40 : 0)), (uint8_t)(id & 127)};
    putV14(a, v);
    return frame(TRACK_PARAM, a);
}

}  // namespace fm1
