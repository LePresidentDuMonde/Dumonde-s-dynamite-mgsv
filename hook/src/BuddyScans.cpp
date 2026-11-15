#include "BuddyScans.h"
#include "BuddyScanMessage.h"
#include "DynamiteHook.h"
#include "LocalBuddy.h"
#include "MinHook.h"
#include "mgsvtpp_func_typedefs.h"
#include <cstring>
#include <windows.h>

namespace Dynamite::BuddyScans {
    namespace {
        using Send=void *(*)(void *,void *,uint32_t,void *);
        Send original=nullptr;
        uintptr_t Base() { return reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)); }
        bool Readable(const void *p,size_t length) {
            MEMORY_BASIC_INFORMATION m{};
            const auto a=reinterpret_cast<uintptr_t>(p);
            return p && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
                a>=reinterpret_cast<uintptr_t>(m.BaseAddress) && length<=m.RegionSize-(a-reinterpret_cast<uintptr_t>(m.BaseAddress));
        }
        template<class T> T Read(const void *p,size_t offset) {
            T result{}; if(p) { const auto a=static_cast<const uint8_t *>(p)+offset; if(Readable(a,sizeof(T))) std::memcpy(&result,a,sizeof(T)); } return result;
        }
        void *Dispatch(void *self,void *result,uint32_t id,void *message) {
            // Stock companion callbacks often query player zero explicitly. In co-op,
            // resolve that owner reference to the local player, inside these callbacks only.
            if(id==0 && LocalBuddy::IsLocalExecution()) id=LocalBuddy::Owner();
            // Only this typed message reveals automatic detections. Its minimum payload is 16 bytes.
            // Do not copy native padding, pointers, or the manual marker duration onto the network.
            if(g_hook && LocalBuddy::IsEnabled() && MergeLocalBuddyReveal(g_hook->dynamiteSyncImpl.buddyScans,id,message)) {
                const auto flags=static_cast<const uint8_t *>(message)[9];
                BuddyRevealPayload merged; merged.flags=1;
                const auto out=original(self,result,id,&merged);
                // The caller still receives the native handled/changed bits, with its input bit intact.
                static_cast<uint8_t *>(message)[9]=(flags&~6u)|(merged.flags&6u);
                return out;
            }
            return original(self,result,id,message);
        }
        bool SendPayload(uint16_t id,BuddyRevealPayload &payload) {
            if(!original || !GetQuarkSystemTable || !FindGameObjectWithID || !FindGameObjectWithID(id)) return false;
            auto table=GetQuarkSystemTable();
            auto manager=Read<void *>(table,0x60);
            // Supplied 1.0.15.4 snapshot: facade vtable 0x142305e78, SendMessage +0x38.
            if(Read<uintptr_t>(manager,0)!=Base()+0x2305e78) return false;
            int64_t result=-1;
            original(manager,&result,id,&payload);
            return (payload.flags&2)!=0;
        }
    }
    bool Install() {
        auto target=reinterpret_cast<void *>(Base()+0xbf3a30);
        constexpr uint8_t expected[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57};
        if(std::memcmp(target,expected,sizeof(expected))!=0 || MH_CreateHook(target,reinterpret_cast<void *>(Dispatch),reinterpret_cast<void **>(&original))!=MH_OK) {
            spdlog::error("buddy scans: native message dispatcher unavailable"); return false;
        }
        if(MH_EnableHook(target)!=MH_OK) { MH_RemoveHook(target); original=nullptr; return false; }
        spdlog::info("buddy scans: native Dog reveals enabled, independent companions, protocol 3"); return true;
    }
    bool Apply(uint16_t id,bool enabled) {
        if(!BuddyScanID(id)) return false;
        BuddyRevealPayload payload; payload.flags=enabled?1:0; return SendPayload(id,payload);
    }
    bool ApplyManual(uint16_t id,float duration) {
        BuddyRevealPayload payload; payload.hash=ManualMarkerMessage; payload.flags=1; payload.reason=0xb8071b05;
        payload.duration=duration==0?15:duration; return SendPayload(id,payload);
    }
}
