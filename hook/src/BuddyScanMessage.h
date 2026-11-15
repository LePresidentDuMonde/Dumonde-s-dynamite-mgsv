#pragma once
#include <cstddef>
#include <cstdint>
#include "DynamiteSyncImpl/BuddyScanState.h"

namespace Dynamite {
    inline constexpr uint64_t BuddyRevealMessage=0xd797144f5e39;
    inline constexpr uint64_t ManualMarkerMessage=0x73f1d2c50cda;
    inline constexpr uint32_t DogScanReason=0x594e8b44;
    struct BuddyRevealPayload {
        uint64_t hash=BuddyRevealMessage;
        uint8_t source=3, flags=0;
        uint16_t padding=0;
        uint32_t reason=DogScanReason;
        float duration=0;
    };
    static_assert(offsetof(BuddyRevealPayload,flags)==9 && offsetof(BuddyRevealPayload,reason)==12 && offsetof(BuddyRevealPayload,duration)==16);
    inline bool MergeLocalBuddyReveal(BuddyScanState &state,uint32_t id,const void *message) {
        // All fields used here are inside the native message's minimum 16-byte extent.
        if(!message || *static_cast<const uint64_t *>(message)!=BuddyRevealMessage ||
            static_cast<const uint8_t *>(message)[8]!=3 || !BuddyScanID(id)) return false;
        const auto p=static_cast<const uint8_t *>(message);
        return state.Observe(id,p[9]&1,*reinterpret_cast<const uint32_t *>(p+12)==DogScanReason);
    }
}
