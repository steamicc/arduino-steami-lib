// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

class SteamiScreen;

/// Keeps the last N readings for screen.graph(); a new value pushes out the oldest. It holds no
/// heap memory: 4N + 4 bytes in the sketch, N from 1 to 65535.
template <size_t N>
class SteamiHistory {
    static_assert(N >= 1 && N <= 65535, "SteamiHistory holds 1 to 65535 values");

   public:
    /// Stores v as the newest value; once N values are kept, the oldest one is dropped. NAN is
    /// kept too: graph() shows it as a gap.
    void add(float v) {
        _data[_head] = v;
        _head = static_cast<uint16_t>((static_cast<size_t>(_head) + 1) % N);
        if (_count < N) {
            _count = static_cast<uint16_t>(_count + 1);
        }
    }

    /// How many values are stored, at most N.
    size_t size() const { return _count; }

    /// N, the most values the history keeps.
    static constexpr size_t capacity() { return N; }

    /// The i-th value, 0 being the oldest; NAN past size().
    float operator[](size_t i) const {
        if (i >= _count) {
            return NAN;
        }
        return _data[(first() + i) % N];
    }

    /// The newest value; NAN when the history is empty.
    float last() const { return _count == 0 ? static_cast<float>(NAN) : (*this)[_count - 1]; }

    /// Forgets every value.
    void clear() {
        _head = 0;
        _count = 0;
    }

   private:
    friend class SteamiScreen;  // graph() reads the ring in place, with no copy

    /// Index in _data of the oldest value.
    size_t first() const { return (static_cast<size_t>(_head) + N - _count) % N; }

    float _data[N] = {};
    uint16_t _head = 0;   // the slot the next add() writes
    uint16_t _count = 0;  // values stored
};
