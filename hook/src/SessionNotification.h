#pragma once
#include <cstdint>
#include <cstring>

namespace Dynamite {
    // OnSessionNotify at 0x140ae2410 dereferences the member returned by the
    // session's virtual GetMemberInterface(index) without checking for null
    // (0x140ae2754). A queued join notification can outlive that member.
    inline bool CanForwardScriptSessionNotification(void *script, void *session, int type, void *arguments) {
        if (type != 4) return true;
        if (!script || !session || !arguments) return false;
        void *registeredSession = nullptr;
        std::memcpy(&registeredSession, static_cast<const char *>(script) + 0x28, sizeof(registeredSession));
        // Preserve the native early-outs for another session and for clients.
        if (registeredSession != session) return true;
        auto table = *static_cast<void ***>(session);
        if (!table || !table[5] || !table[2]) return false;
        using IsHost = bool (*)(void *);
        if (!reinterpret_cast<IsHost>(table[5])(session)) return true;
        using GetMember = void *(*)(void *, unsigned char);
        return reinterpret_cast<GetMember>(table[2])(session, *static_cast<unsigned char *>(arguments)) != nullptr;
    }
}
