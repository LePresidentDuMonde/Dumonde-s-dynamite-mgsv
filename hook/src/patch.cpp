#include "patch.h"
#include "windows.h"
#include "stdint.h"
#include <algorithm>

bool Patch::Apply() const {
    for (unsigned int i = 0; i < expected.size(); i++) {
        if (*(uint8_t *)(address + i) != expected[i]) {
            return false;
        }
    }

    const auto size = std::max(expected.size(), patch.size());
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    for (unsigned int i = 0; i < patch.size(); i++) {
        if (patch[i] >= 0 && patch[i] <= 0xFF) {
            *(uint8_t *)(address + i) = (uint8_t)patch[i];
        }
    }

    return VirtualProtect(reinterpret_cast<LPVOID>(address), size, oldProtect, &oldProtect);
}
