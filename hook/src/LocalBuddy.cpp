#include "LocalBuddy.h"
#include "BuddyOwnership.h"
#include "DogReplica.h"
#include "MinHook.h"
#include "Tpp/TppGameObject.h"
#include "spdlog/spdlog.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <windows.h>

namespace Dynamite::LocalBuddy {
    namespace {
        std::atomic<bool> enabled = false;
        std::atomic<bool> installed = false;
        thread_local bool localCreation = false;
        thread_local bool localExecution = false;
        std::atomic<uint16_t> owner = 0;
        struct ExecutionScope {
            bool previous=localExecution;
            ExecutionScope() { localExecution=enabled; }
            ~ExecutionScope() { localExecution=previous; }
        };
        using DogUpdate=void (*)(void *);
        using DogJob=void (*)(void *,void *);
        DogUpdate originalDogPre=nullptr, originalDogPost=nullptr;
        DogJob originalDogJob=nullptr;
        void DogPreHook(void *self) {
            ExecutionScope scope; DogReplica::BeforePre(self); originalDogPre(self); DogReplica::AfterPre(self);
        }
        void DogPostHook(void *self) { ExecutionScope scope; originalDogPost(self); }
        void DogJobHook(void *self,void *task) { ExecutionScope scope; originalDogJob(self,task); }
        using CreateType = int64_t (*)(void *, uint32_t *);
        using HorseMessage = void (*)(void *, void *, uint32_t, const uint64_t *);
        CreateType originalCreate = nullptr;
        HorseMessage originalHorseMessage = nullptr;
        using DogCreate = void (*)(void *,void *);
        using DogMessage = void (*)(void *,int32_t *,uint32_t,const uint64_t *);
        DogCreate originalDogCreate=nullptr;
        DogMessage originalDogMessage=nullptr;

        void DogCreateHook(void *self, void *description) {
            DogReplica::Created();
            const bool previous=localCreation;
            localCreation=enabled;
            struct Restore { bool previous; ~Restore(){localCreation=previous;} } restore{previous};
            originalDogCreate(self,description);
        }
        void DogMessageHook(void *self,int32_t *result,uint32_t index,const uint64_t *message) {
            // Slot 1 is a passive visual copy, never a second locally commanded buddy.
            if(index==1 && *reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(self)+8)==2 && message && message[0]!=0xd0a5d0c7e373 && message[0]!=0xa40fa49ee6da) {
                if(result) *result=-1; return;
            }
            ExecutionScope scope;
            alignas(16) uint8_t local[32]{};
            if(enabled && index==0) message=PersonalDogCommand(message,owner,local);
            originalDogMessage(self,result,index,message);
        }

        int64_t CreateTypeHook(void *type, uint32_t *description) {
            const auto index = *reinterpret_cast<const uint16_t *>(static_cast<const uint8_t *>(type) + 0x1c);
            const bool previous = localCreation;
            // The scene owns the pool size. Only local companion actors receive the offline
            // creation descriptor, never soldiers, players or mission objects.
            localCreation = enabled && (index == GAME_OBJECT_TYPE_HORSE2 || index == GAME_OBJECT_TYPE_BUDDYDOG2);
            struct Restore { bool previous; ~Restore() { localCreation = previous; } } restore{previous};
            return originalCreate(type, description);
        }

        void HorseMessageHook(void *self, void *result, uint32_t index, const uint64_t *message) {
            ExecutionScope scope;
            // The decompiled Horse receiver rejects whistles solely because c4 bit 1 is set online.
            // Scope the bypass to a live local horse and this one input; keep every other state bit.
            if (!enabled || index != 0 || !message || message[0] != 0x7c8cc24e84c3 ||
                *reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(self) + 0xc) == 0) {
                return originalHorseMessage(self, result, index, message);
            }
            auto &flags = *reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(self) + 0xc4);
            const auto onlineFlag = flags & 2u;
            flags &= ~2u;
            alignas(16) uint64_t local[2]={message[0],owner.load()};
            originalHorseMessage(self, result, index, local);
            flags = (flags & ~2u) | onlineFlag;
        }
    }

    bool Install() {
        const auto target = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) + 0x9e5470);
        const auto horseTarget = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)) + 0xe4d960);
        const auto dogCreate=reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr))+0xa58310);
        const auto dogMessage=reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr))+0xc56b10);
        const auto dogPre=reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr))+0xc55690);
        const auto dogPost=reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr))+0xc55580);
        const auto dogJob=reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr))+0xc55530);
        // Corroborated by mgsvtpp.exe.ranges.jsonl.gz, entry 0x1409e5470.
        constexpr uint8_t expected[] = {0x48, 0x89, 0x54, 0x24, 0x10};
        constexpr uint8_t horseExpected[] = {0x40, 0x55, 0x57, 0x41, 0x54};
        constexpr uint8_t dogCreateExpected[]={0x40,0x55,0x56,0x57,0x41,0x54};
        constexpr uint8_t dogMessageExpected[]={0x40,0x53,0x57,0x41,0x56};
        constexpr uint8_t preExpected[]={0x40,0x53,0x56,0x48,0x83,0xec,0x28};
        constexpr uint8_t postExpected[]={0x48,0x89,0x5c,0x24,0x10,0x57};
        constexpr uint8_t jobExpected[]={0xf6,0x81,0xd0,0,0,0,1};
        if (std::memcmp(target, expected, sizeof(expected)) != 0 || std::memcmp(horseTarget, horseExpected, sizeof(horseExpected)) != 0 ||
            std::memcmp(dogCreate,dogCreateExpected,sizeof(dogCreateExpected))!=0 || std::memcmp(dogMessage,dogMessageExpected,sizeof(dogMessageExpected))!=0 ||
            std::memcmp(dogPre,preExpected,sizeof(preExpected))!=0 || std::memcmp(dogPost,postExpected,sizeof(postExpected))!=0 ||
            std::memcmp(dogJob,jobExpected,sizeof(jobExpected))!=0 ||
            MH_CreateHook(target, reinterpret_cast<void *>(CreateTypeHook), reinterpret_cast<void **>(&originalCreate)) != MH_OK) {
            spdlog::error("local companions: creation hook unavailable; stock behavior retained");
            return false;
        }
        if (MH_CreateHook(horseTarget, reinterpret_cast<void *>(HorseMessageHook), reinterpret_cast<void **>(&originalHorseMessage)) != MH_OK) {
            MH_RemoveHook(target);
            return false;
        }
        if (MH_CreateHook(dogCreate,reinterpret_cast<void *>(DogCreateHook),reinterpret_cast<void **>(&originalDogCreate))!=MH_OK ||
            MH_CreateHook(dogMessage,reinterpret_cast<void *>(DogMessageHook),reinterpret_cast<void **>(&originalDogMessage))!=MH_OK ||
            MH_CreateHook(dogPre,reinterpret_cast<void *>(DogPreHook),reinterpret_cast<void **>(&originalDogPre))!=MH_OK ||
            MH_CreateHook(dogPost,reinterpret_cast<void *>(DogPostHook),reinterpret_cast<void **>(&originalDogPost))!=MH_OK ||
            MH_CreateHook(dogJob,reinterpret_cast<void *>(DogJobHook),reinterpret_cast<void **>(&originalDogJob))!=MH_OK ||
            MH_EnableHook(target) != MH_OK || MH_EnableHook(horseTarget) != MH_OK || MH_EnableHook(dogCreate)!=MH_OK || MH_EnableHook(dogMessage)!=MH_OK) {
            MH_DisableHook(target);
            MH_DisableHook(horseTarget);
            MH_RemoveHook(target);
            MH_RemoveHook(horseTarget);
            MH_DisableHook(dogCreate); MH_DisableHook(dogMessage);
            MH_RemoveHook(dogCreate); MH_RemoveHook(dogMessage);
            for(auto t:{dogPre,dogPost,dogJob}) { MH_DisableHook(t); MH_RemoveHook(t); }
            return false;
        }
        if(MH_EnableHook(dogPre)!=MH_OK || MH_EnableHook(dogPost)!=MH_OK || MH_EnableHook(dogJob)!=MH_OK) {
            for(auto t:{target,horseTarget,dogCreate,dogMessage,dogPre,dogPost,dogJob}) { MH_DisableHook(t); MH_RemoveHook(t); }
            return false;
        }
        spdlog::info("local companions: scene-defined pools, scoped offline creation");
        installed=true;
        return true;
    }

    void SetEnabled(bool value) { enabled = value; if(!value) owner=0; }
    bool IsLocalCreation() { return localCreation; }
    bool Available() { return installed; }
    bool IsEnabled() { return enabled; }
    bool IsLocalExecution() { return localExecution; }
    uint16_t Owner() { return owner; }
    void UpdateOwner(bool host,bool connected) { owner=PersonalBuddyOwner(host,connected); }
}
