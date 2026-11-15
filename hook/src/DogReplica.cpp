#include "DogReplica.h"
#include "DynamiteHook.h"
#include "LocalBuddy.h"
#include "MinHook.h"
#include "mgsvtpp_func_typedefs.h"
#include <atomic>
#include <cstring>
#include <windows.h>

namespace Dynamite::DogReplica {
    namespace {
        std::atomic<bool> installed=false, active=false, rejected=false;
        std::atomic<uint32_t> generation=1;
        std::atomic<uint32_t> rejectedGeneration=0;
        // These addresses are compared only. Every use reacquires the current Dog graph.
        std::atomic<void *> localSkeleton=nullptr, remoteSkeleton=nullptr;
        using ModelCommit=void (*)(void *);
        ModelCommit originalCommit=nullptr, originalPose=nullptr;
        uintptr_t Base() { return reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)); }
        bool Memory(const void *p,size_t length,bool write=false) {
            MEMORY_BASIC_INFORMATION m{}; const auto a=reinterpret_cast<uintptr_t>(p);
            return p && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT && !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) &&
                (!write || m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) &&
                a>=reinterpret_cast<uintptr_t>(m.BaseAddress) && length<=m.RegionSize-(a-reinterpret_cast<uintptr_t>(m.BaseAddress));
        }
        template<class T> T Read(const void *p,size_t offset) {
            T result{}; if(p) { auto at=static_cast<const uint8_t *>(p)+offset; if(Memory(at,sizeof(T))) std::memcpy(&result,at,sizeof(T)); } return result;
        }
        void *Method(void *self,size_t offset) {
            auto method=Read<void *>(Read<void *>(self,0),offset);
            MEMORY_BASIC_INFORMATION m{};
            if(!method || !VirtualQuery(method,&m,sizeof(m)) || m.AllocationBase!=GetModuleHandle(nullptr) ||
                !(m.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))) return nullptr;
            return method;
        }
        void *Root() {
            if(!FindGameObjectWithID) return nullptr;
            auto wrapper=FindGameObjectWithID(19*512);
            if(Read<uintptr_t>(wrapper,0)!=Base()+0x22bdf30) return nullptr;
            auto quark=Read<void *>(Read<void *>(wrapper,0x80),0x10);
            auto query=Method(quark,0x18); if(!query) return nullptr;
            auto result=reinterpret_cast<void *(*)(void *,uint64_t)>(query)(quark,0xd6974413c33);
            if(!result) return nullptr;
            auto root=static_cast<uint8_t *>(result)-8;
            return Read<uintptr_t>(root,0)==Base()+0x2315c70 && Read<uint32_t>(root,8)==2 ? root : nullptr;
        }
        void *Model(void *root,uint32_t index,uint32_t *physicalOut=nullptr) {
            auto pool=Read<void *>(root,0x10);
            const auto count=Read<uint32_t>(pool,0x44), perGroup=Read<uint32_t>(root,0x108);
            if(!count || count>2 || !perGroup) return nullptr;
            auto map=Read<void *>(pool,0x28);
            if(!Memory(map,count*sizeof(uint16_t))) return nullptr;
            std::array<uint16_t,2> mapping{0xffff,0xffff}; std::memcpy(mapping.data(),map,count*sizeof(uint16_t));
            const auto physical=DogPhysicalSlot(mapping,count,index); if(!physical) return nullptr;
            // Same logical-to-realized lookup as stock Dog getter 0x140c55ac0.
            auto groups=Read<void *>(root,0x110);
            auto group=Read<void *>(groups,8*(*physical/perGroup));
            auto model=Read<void *>(group,0x10);
            if(Read<uintptr_t>(model,0)!=Base()+0x22e6070) return nullptr;
            const auto first=Read<uint32_t>(model,0xc), modelCount=Read<uint32_t>(model,8);
            if(*physical<first || *physical-first>=modelCount) return nullptr;
            if(physicalOut) *physicalOut=*physical;
            return model;
        }
        uint8_t *Record(void *model,uint32_t index) {
            auto records=Read<uint8_t *>(model,0x10); const auto first=Read<uint32_t>(model,0xc);
            if(!records || index<first || index-first>=Read<uint32_t>(model,8)) return nullptr;
            auto record=records+(index-first)*0xc0;
            return Memory(record,0xc0)?record:nullptr;
        }
        void *Character(void *model,uint32_t index) {
            auto method=Method(model,0x388); if(!method || !Record(model,index)) return nullptr;
            auto character=reinterpret_cast<void *(*)(void *,uint32_t)>(method)(model,index);
            return Memory(character,0x70)?character:nullptr;
        }
        bool PoseMemory(void *skeleton,uint16_t &count,void *&bones,uint8_t &space) {
            // Corroborated by 0x140af0e10 and the Gr model view at 0x140215cd0.
            count=Read<uint16_t>(skeleton,0xf8); bones=Read<void *>(skeleton,0xe0); space=Read<uint8_t>(skeleton,0xfa)&1;
            return count && count<=MaxDogBones && !(reinterpret_cast<uintptr_t>(bones)&15) && Memory(skeleton,0x1c4) && Memory(bones,count*sizeof(DogMatrix));
        }
        void Capture(void *skeleton,void *model,uint32_t physical) {
            DogPose pose; uint16_t count; void *bones;
            if(!PoseMemory(skeleton,count,bones,pose.space)) return;
            auto character=Character(model,physical); if(!character) return;
            pose.position=Read<std::array<float,3>>(character,0x20);
            pose.rotation=Read<std::array<float,4>>(character,0x60);
            pose.active=true; pose.generation=generation;
            pose.world=Read<DogMatrix>(skeleton,0xa0); pose.bones.resize(count);
            std::memcpy(pose.bones.data(),bones,count*sizeof(DogMatrix));
            g_hook->dynamiteSyncImpl.dogReplica.Capture(std::move(pose),std::chrono::steady_clock::now());
        }
        bool Apply(void *skeleton) {
            auto pose=g_hook->dynamiteSyncImpl.dogReplica.Peer(std::chrono::steady_clock::now());
            uint16_t count; void *bones; uint8_t space;
            if(!pose || !PoseMemory(skeleton,count,bones,space)) return false;
            if(count!=pose->bones.size() || space!=pose->space) {
                rejectedGeneration=pose->generation;
                if(!rejected.exchange(true)) spdlog::error("Dog replica: incompatible skeleton; hiding the remote actor");
                return false;
            }
            if(!Memory(skeleton,0x1c4,true) || !Memory(bones,count*sizeof(DogMatrix),true)) return false;
            // Stock Gr setter updates both transforms and the scale bound. Copy only matrices,
            // never a remote descriptor, pointer, vtable, command buffer, or allocation count.
            if(!CopyDogMatrices(bones,count,space,*pose)) return false;
            reinterpret_cast<void (*)(void *,const float *)>(Base()+0x1ce7f0)(skeleton,pose->world.data());
            return true;
        }
        void CommitHook(void *self) {
            if(active && g_hook && LocalBuddy::IsLocalExecution()) {
                auto root=Root();
                if(root) {
                    uint32_t physical=0;
                    if(auto model=Model(root,0,&physical);model==self) {
                        auto rec=Record(self,physical); if(rec) { auto skeleton=Read<void *>(rec,0x68); localSkeleton=skeleton; Capture(skeleton,self,physical); }
                    }
                    if(auto model=Model(root,1,&physical);model==self && !rejected) {
                        auto rec=Record(self,physical); if(rec) { auto skeleton=Read<void *>(rec,0x68); remoteSkeleton=skeleton; Apply(skeleton); }
                    }
                }
            }
            originalCommit(self);
        }
        void PoseHook(void *self) {
            originalPose(self);
            if(!active || !g_hook) return;
            if(self!=localSkeleton.load() && self!=remoteSkeleton.load()) return;
            auto root=Root(); if(!root) return;
            uint32_t localIndex=0,remoteIndex=0;
            auto localModel=Model(root,0,&localIndex), remoteModel=Model(root,1,&remoteIndex);
            auto local=Record(localModel,localIndex), remote=Record(remoteModel,remoteIndex);
            if(local && Read<void *>(local,0x68)==self) Capture(self,localModel,localIndex);
            if(remote && Read<void *>(remote,0x68)==self && !rejected) Apply(self);
        }
    }
    bool Install() {
        if(!LocalBuddy::Available()) { spdlog::error("Dog replicas: local companion hooks unavailable"); return false; }
        auto commit=reinterpret_cast<void *>(Base()+0xaf0240), pose=reinterpret_cast<void *>(Base()+0x1ce260);
        constexpr uint8_t commitBytes[]={0x40,0x56,0x48,0x83,0xec,0x20};
        // The exact pose finalizer prologue is checked before any hook is enabled.
        constexpr uint8_t poseBytes[]={0x40,0x53,0x48,0x83,0xec,0x70};
        constexpr uint8_t worldBytes[]={0x40,0x53,0x48,0x83,0xec,0x20,0x0f,0x28,0x02};
        if(std::memcmp(commit,commitBytes,sizeof(commitBytes)) || std::memcmp(pose,poseBytes,sizeof(poseBytes)) ||
           std::memcmp(reinterpret_cast<void *>(Base()+0x1ce7f0),worldBytes,sizeof(worldBytes))) {
            spdlog::error("Dog replica: unsupported native model entry points"); return false;
        }
        if(MH_CreateHook(commit,reinterpret_cast<void *>(CommitHook),reinterpret_cast<void **>(&originalCommit))!=MH_OK ||
           MH_CreateHook(pose,reinterpret_cast<void *>(PoseHook),reinterpret_cast<void **>(&originalPose))!=MH_OK ||
           MH_EnableHook(commit)!=MH_OK || MH_EnableHook(pose)!=MH_OK) {
            for(auto target:{commit,pose}) { MH_DisableHook(target); MH_RemoveHook(target); } return false;
        }
        installed=true; spdlog::info("Dog replicas: local actor 0, passive remote actor 1, skeletal poses, protocol 4"); return true;
    }
    void Created() { if(++generation==0) ++generation; rejected=false; rejectedGeneration=0; localSkeleton=remoteSkeleton=nullptr; }
    bool Available() { return installed; }
    uint32_t Generation() { return generation; }
    void BeforePre(void *root) {
        // Keep the extra slot disabled even if model replication cannot be installed.
        if(Read<uint32_t>(root,8)!=2) return;
        auto flags=Read<uint8_t *>(Read<void *>(root,0x80),8);
        if(!flags || !Memory(flags+16,8,true)) return;
        auto value=Read<uint64_t>(flags,16);
        // Stock SetEnabled(false): disabled bit 59, dirty bit 60. Actor zero is untouched.
        if(!(value&(uint64_t{1}<<59))) {
            value|=(uint64_t{1}<<59)|(uint64_t{1}<<60); std::memcpy(flags+16,&value,8);
        }
    }
    void AfterPre(void *root) {
        if(!installed || Read<uint32_t>(root,8)!=2) return;
        auto requested=Read<uint8_t *>(Read<void *>(root,0x10),0);
        if(!requested || !Memory(requested,1,true)) return;
        auto peer=active && g_hook ? g_hook->dynamiteSyncImpl.dogReplica.Peer(std::chrono::steady_clock::now()) : std::nullopt;
        if(rejected && peer && peer->generation!=rejectedGeneration) rejected=false;
        const bool visible=active && !rejected && peer.has_value();
        if(visible) {
            auto pool=Read<void *>(root,0x10);
            auto saved=Read<uint8_t *>(pool,0x20);
            if(saved && Memory(saved+16,12,true)) std::memcpy(saved+16,peer->position.data(),12);
            uint32_t physical=0;
            auto model=Model(root,1,&physical);
            auto character=Character(model,physical);
            auto set=Method(character,0x68);
            if(set) {
                alignas(16) float position[4]={peer->position[0],peer->position[1],peer->position[2],0};
                alignas(16) float rotation[4]={peer->rotation[0],peer->rotation[1],peer->rotation[2],peer->rotation[3]};
                // Same local transform ABI used by stock Player2::Warp (0x1409ceae0).
                // This runs in the pre-update, before the actor's physics/model jobs.
                reinterpret_cast<void (*)(void *,const float *,const float *)>(set)(character,position,rotation);
            }
        }
        // Only realize the passive model. The native disabled command flag stays set, so
        // its companion AI, attacks, affinity and scans are not executed a second time.
        *requested=(*requested&~2u)|(visible?2:0);
    }
    void Update(bool ready,bool selected) {
        active=installed && ready && selected && LocalBuddy::IsEnabled();
        if(!active && g_hook) g_hook->dynamiteSyncImpl.dogReplica.Reset();
    }
    void ResetWorld() { active=false; Created(); }
}
