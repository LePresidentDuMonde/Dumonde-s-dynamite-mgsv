#pragma once
#include <cstdint>
#include <cstring>

namespace Dynamite {
    // Player indices are canonical on both peers; the local solo player remains zero.
    inline uint16_t PersonalBuddyOwner(bool host, bool connected) { return connected && !host ? 1 : 0; }
    inline const uint64_t *PersonalDogCommand(const uint64_t *message,uint16_t owner,uint8_t (&local)[32]) {
        if(!message) return message;
        if(message[0]==0xf28e1c4cc23a) {
            std::memcpy(local,message,10); std::memcpy(local+8,&owner,2);
        } else if(message[0]==0x12b9a4e6da92 || message[0]==0x2ea8dc64967c) {
            // Verified target/player/opaque pointer/option layout, preserved on this machine.
            std::memcpy(local,message,25); std::memcpy(local+10,&owner,2);
        } else return message;
        return reinterpret_cast<const uint64_t *>(local);
    }
}
