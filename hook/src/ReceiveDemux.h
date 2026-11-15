#pragma once
#include <cstdint>
#include <cstring>

inline constexpr char DYNAMITE_RAW_HEADER[8] = {'D','Y','N','A','M','I','T','E'};

namespace Dynamite {
    // The native Steam receiver returns -1 when no datagram is available.
    // Private mod frames must not enter MGSV's native packet decoder. Continue
    // reading so a mod frame does not hide a queued native movement packet.
    template<class Read, class Consume>
    int32_t ReceiveGameDatagram(void *buffer, int capacity, Read read, Consume consume) {
        for (unsigned i=0; i<32; ++i) {
            const int32_t size=read();
            if (size<=0) return size;
            if (!buffer || size>capacity) return -1;
            if (size<static_cast<int32_t>(sizeof(DYNAMITE_RAW_HEADER)) ||
                std::memcmp(buffer,DYNAMITE_RAW_HEADER,sizeof(DYNAMITE_RAW_HEADER))!=0) return size;
            consume(buffer,size);
        }
        // A burst of private frames is bounded to avoid stalling the network job.
        return -1;
    }
}
