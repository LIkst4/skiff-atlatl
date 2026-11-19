/* SPDX-License-Identifier: GPL-3.0-only */
/* A single-producer single-consumer ring (the audio thread to the link's), lock-free, fixed size. */
#pragma once
#include <atomic>
#include <cstddef>

namespace fm1 {

template <typename T, size_t N> class Ring {
    static_assert((N & (N - 1)) == 0, "N a power of two");
    T buf[N];
    std::atomic<size_t> w{0}, r{0};

public:
    bool push(const T &v)                               /* the producer; false when full (the item is dropped) */
    {
        size_t i = w.load(std::memory_order_relaxed);
        if (i - r.load(std::memory_order_acquire) >= N)
            return false;
        buf[i & (N - 1)] = v;
        w.store(i + 1, std::memory_order_release);
        return true;
    }
    bool pop(T &v)                                      /* the consumer */
    {
        size_t i = r.load(std::memory_order_relaxed);
        if (i == w.load(std::memory_order_acquire))
            return false;
        v = buf[i & (N - 1)];
        r.store(i + 1, std::memory_order_release);
        return true;
    }
};

}  // namespace fm1
