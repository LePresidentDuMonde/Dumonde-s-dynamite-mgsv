#pragma once
#include <cstdint>

namespace Dynamite::BuddyScans {
    bool Install();
    bool Apply(uint16_t id,bool enabled);
    bool ApplyManual(uint16_t id,float duration);
}
