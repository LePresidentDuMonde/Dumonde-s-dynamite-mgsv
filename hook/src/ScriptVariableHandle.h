#pragma once
#include <cstdint>

// Native GetScriptDeclVarHandle writes bytes 0..12, including on lookup failure.
// See the decompilation at 0x1409abe20. The previous eight-byte allocation was too small.
struct alignas(8) ScriptVariableHandle {
    uint64_t base = 0;
    uint16_t count = 0;
    uint16_t start = 0;
    uint8_t flags = 0;
    uint8_t reserved[3]{};
};
static_assert(sizeof(ScriptVariableHandle) == 16);
