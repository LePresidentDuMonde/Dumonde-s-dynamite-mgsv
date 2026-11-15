#pragma once
#include <cstdint>

namespace Dynamite::LocalBuddy {
    bool Install();
    void SetEnabled(bool enabled);
    bool IsLocalCreation();
    bool Available();
    bool IsEnabled();
    bool IsLocalExecution();
    uint16_t Owner();
    void UpdateOwner(bool host, bool connected);
}
