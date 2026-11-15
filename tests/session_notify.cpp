#include "SessionNotification.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
    struct Session { void **table; bool host; void *member; unsigned char requested = 0; int lookups = 0; };
    bool IsHost(void *self) { return static_cast<Session *>(self)->host; }
    void *GetMember(void *self, unsigned char index) {
        auto &session = *static_cast<Session *>(self);
        session.requested = index;
        ++session.lookups;
        return session.member;
    }
    void Check(bool value, const char *text) { if (!value) throw std::runtime_error(text); }
}
int main() {
    try {
        std::array<void *,6> vtable{};
        vtable[2]=reinterpret_cast<void *>(&GetMember);
        vtable[5]=reinterpret_cast<void *>(&IsHost);
        Session session{vtable.data(),true,nullptr};
        std::array<unsigned char,0x38> script{};
        void *registered=&session;
        std::memcpy(script.data()+0x28,&registered,sizeof(registered));
        unsigned char index=1;
        auto forward=[&](int type=4,void *args=nullptr) {
            return Dynamite::CanForwardScriptSessionNotification(script.data(),&session,type,args ? args : &index);
        };
        Check(!forward(),"stale member reached native null dereference");
        Check(session.lookups==1 && session.requested==1,"wrong native member queried");
        session.member=&session;
        Check(forward(),"valid join was suppressed");
        session.member=nullptr;
        Check(!forward(),"departed member was reused");
        session.host=false;
        int before=session.lookups;
        Check(forward() && session.lookups==before,"client callback did not preserve native early-out");
        session.host=true;
        registered=nullptr;
        std::memcpy(script.data()+0x28,&registered,sizeof(registered));
        Check(forward() && session.lookups==before,"unrelated session queried a member");
        Check(Dynamite::CanForwardScriptSessionNotification(nullptr,nullptr,1,nullptr),"session-close notification suppressed");
        Check(!Dynamite::CanForwardScriptSessionNotification(script.data(),&session,4,nullptr),"null join arguments accepted");
        std::cout << "PASS: script-session join rejects missing/departed members; valid joins, client and close notifications preserved\n";
    } catch (const std::exception &error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
