#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Dynamite {
    inline constexpr size_t MaxSyncPayload = 1024;

    inline bool FinitePosition(float x, float y, float z, float angle) {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(angle) &&
               std::abs(x) < 100000.f && std::abs(y) < 100000.f && std::abs(z) < 100000.f;
    }

    inline bool VehicleID(uint32_t id) { return id < 0xffff && ((id >> 9) == 50 || (id >> 9) == 15); }
    inline bool CarryID(uint32_t id) { return id < 0xffff && (id >> 9) >= 2 && (id >> 9) <= 6; }

    // Accept reordering without allowing duplicates or an old state to undo a newer one.
    // Sequence zero is reserved. Unsigned subtraction also handles uint32 wraparound.
    inline bool NewerSequence(uint32_t value, uint32_t previous) {
        return value != 0 && (previous == 0 || (value != previous && uint32_t(value - previous) < 0x80000000u));
    }

    struct ReplayWindow {
        uint32_t highest = 0;
        uint64_t seen = 0;
        bool Accept(uint32_t value) {
            if (value == 0) return false;
            if (NewerSequence(value, highest)) {
                const uint32_t distance = value - highest;
                seen = distance >= 64 ? 1 : (seen << distance) | 1;
                highest = value;
                return true;
            }
            const uint32_t distance = highest - value;
            if (distance >= 64 || (seen & (uint64_t{1} << distance))) return false;
            seen |= uint64_t{1} << distance;
            return true;
        }
    };
}
