// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <new>
#include <utility>

/// Storage for one object that is built on demand, without heap allocation.
template <typename T>
class SteamiLazy {
   public:
    constexpr SteamiLazy() = default;

    /// Builds the object on the first call; later calls return the existing one.
    template <typename... A>
    T& emplace(A&&... args) {
        if (!_built) {
            new (static_cast<void*>(_storage)) T(std::forward<A>(args)...);
            _built = true;
        }
        return *get();
    }

    /// Returns the object, or nullptr if it has not been built.
    T* get() { return _built ? reinterpret_cast<T*>(_storage) : nullptr; }

    bool built() const { return _built; }

    /// Destroys the object, if any.
    void reset() {
        if (_built) {
            get()->~T();
            _built = false;
        }
    }

   private:
    alignas(T) unsigned char _storage[sizeof(T)] = {};
    bool _built = false;
};
