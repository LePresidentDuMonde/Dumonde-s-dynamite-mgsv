#pragma once
#include "SyncPolicy.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <vector>

namespace Dynamite {
    inline constexpr uint16_t MaxDogBones=256, DogBonesPerChunk=10;
    struct alignas(16) DogMatrix : std::array<float,16> {};
    struct DogPose {
        uint32_t generation=1, frame=0;
        uint8_t owner=0, space=0;
        bool active=false;
        DogMatrix world{};
        std::array<float,3> position{};
        std::array<float,4> rotation{0,0,0,1};
        std::vector<DogMatrix> bones;
    };
    struct DogPosePart { DogPose pose; uint16_t boneCount=0, firstBone=0; };
    inline std::optional<uint16_t> DogPhysicalSlot(const std::array<uint16_t,2> &map,uint32_t count,uint16_t logical) {
        if(!count || count>map.size() || logical>1) return {};
        std::optional<uint16_t> found;
        for(uint16_t i=0;i<count;++i) if(map[i]==logical) { if(found) return {}; found=i; }
        return found;
    }
    inline bool ValidDogMatrix(const DogMatrix &m,bool world=false) {
        for(float value:m) if(!std::isfinite(value) || std::abs(value)>=100000.f) return false;
        if(std::abs(m[3])>.01f || std::abs(m[7])>.01f || std::abs(m[11])>.01f || std::abs(m[15]-1)>.01f) return false;
        if(world) {
            const float det=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
            if(!std::isfinite(det) || std::abs(det)<.01f || std::abs(det)>100.f) return false;
        }
        return true;
    }
    inline bool ValidDogPart(const DogPosePart &p) {
        const auto &v=p.pose;
        if(!v.generation || !v.frame || v.owner>1 || v.space>1) return false;
        if(!v.active) return p.boneCount==0 && p.firstBone==0 && v.bones.empty() && v.space==0;
        float norm=0; for(float q:v.rotation) { if(!std::isfinite(q) || std::abs(q)>1.01f) return false; norm+=q*q; }
        if(norm<.5f || norm>1.5f || !FinitePosition(v.position[0],v.position[1],v.position[2],0)) return false;
        if(!p.boneCount || p.boneCount>MaxDogBones || p.firstBone>=p.boneCount || p.firstBone%DogBonesPerChunk ||
           v.bones.size()!=std::min<size_t>(DogBonesPerChunk,p.boneCount-p.firstBone) || !ValidDogMatrix(v.world,true)) return false;
        for(const auto &bone:v.bones) if(!ValidDogMatrix(bone)) return false;
        return true;
    }
    inline bool CopyDogMatrices(void *destination,uint16_t nativeCount,uint8_t nativeSpace,const DogPose &pose) {
        if(!destination || !pose.active || !nativeCount || nativeCount>MaxDogBones || nativeCount!=pose.bones.size() ||
           nativeSpace!=pose.space || !ValidDogMatrix(pose.world,true)) return false;
        for(const auto &bone:pose.bones) if(!ValidDogMatrix(bone)) return false;
        std::memcpy(destination,pose.bones.data(),nativeCount*sizeof(DogMatrix)); return true;
    }
    // One bounded pending frame and one completed frame; no native pointer is stored.
    class DogReplicaState {
        std::mutex mutex;
        std::optional<DogPose> local, peer, previous;
        DogPose assembling;
        std::array<bool,(MaxDogBones+DogBonesPerChunk-1)/DogBonesPerChunk> chunks{};
        uint32_t newest=0, generation=0, localFrame=0;
        std::chrono::steady_clock::time_point localTime{}, peerTime{};
      public:
        void Capture(DogPose pose,std::chrono::steady_clock::time_point now) {
            if(!pose.active || pose.bones.empty() || pose.bones.size()>MaxDogBones || !ValidDogMatrix(pose.world,true)) return;
            for(const auto &bone:pose.bones) if(!ValidDogMatrix(bone)) return;
            std::lock_guard lock(mutex); local=std::move(pose); localTime=now;
        }
        DogPose Outgoing(bool selected,uint8_t owner,uint32_t currentGeneration,std::chrono::steady_clock::time_point now) {
            std::lock_guard lock(mutex);
            DogPose value;
            if(selected && local && local->generation==currentGeneration && now-localTime<std::chrono::milliseconds(500)) value=*local;
            value.owner=owner; value.generation=currentGeneration;
            if(++localFrame==0) ++localFrame; value.frame=localFrame;
            return value;
        }
        bool Receive(const DogPosePart &p,std::chrono::steady_clock::time_point now) {
            if(!ValidDogPart(p)) return false;
            std::lock_guard lock(mutex);
            const auto &v=p.pose;
            if(generation && v.generation!=generation && !NewerSequence(v.generation,generation)) return false;
            if(v.frame!=newest) {
                if(!NewerSequence(v.frame,newest)) return false;
                newest=v.frame; generation=v.generation; assembling=v; assembling.bones.resize(p.boneCount); chunks.fill(false);
            } else if(!v.active || peer && peer->frame==v.frame) return false;
            if(!v.active) { previous.reset(); peer=v; peerTime=now; return true; }
            if(assembling.generation!=v.generation || assembling.owner!=v.owner || assembling.space!=v.space ||
               assembling.bones.size()!=p.boneCount || assembling.world!=v.world || assembling.position!=v.position ||
               assembling.rotation!=v.rotation || !assembling.active) return false;
            const auto index=p.firstBone/DogBonesPerChunk;
            if(chunks[index]) return false;
            std::copy(v.bones.begin(),v.bones.end(),assembling.bones.begin()+p.firstBone); chunks[index]=true;
            const size_t count=(p.boneCount+DogBonesPerChunk-1)/DogBonesPerChunk;
            for(size_t i=0;i<count;++i) if(!chunks[i]) return false;
            previous=peer;
            if(previous && (previous->generation!=v.generation || previous->bones.size()!=p.boneCount || previous->space!=v.space)) previous.reset();
            peer=assembling; peerTime=now; return true;
        }
        std::optional<DogPose> Peer(std::chrono::steady_clock::time_point now) {
            std::lock_guard lock(mutex);
            if(!peer || !peer->active || now-peerTime>std::chrono::seconds(3)) return {};
            auto sample=*peer;
            if(previous && previous->active) {
                float distance=0; for(int i=12;i<15;++i) { const auto d=previous->world[i]-sample.world[i]; distance+=d*d; }
                if(distance<400.f) {
                    const float alpha=std::clamp(std::chrono::duration<float>(now-peerTime).count()/.05f,0.f,1.f);
                    for(int i=0;i<16;++i) sample.world[i]=previous->world[i]+alpha*(sample.world[i]-previous->world[i]);
                    if(!ValidDogMatrix(sample.world,true)) sample.world=peer->world;
                    for(int i=0;i<3;++i) sample.position[i]=previous->position[i]+alpha*(sample.position[i]-previous->position[i]);
                    float dot=0,norm=0; for(int i=0;i<4;++i) dot+=previous->rotation[i]*sample.rotation[i];
                    for(int i=0;i<4;++i) { sample.rotation[i]=previous->rotation[i]+alpha*((dot<0?-sample.rotation[i]:sample.rotation[i])-previous->rotation[i]); norm+=sample.rotation[i]*sample.rotation[i]; }
                    if(norm>.01f) for(float &q:sample.rotation) q/=std::sqrt(norm); else sample.rotation=peer->rotation;
                    for(size_t bone=0;bone<sample.bones.size();++bone) for(int i=0;i<16;++i)
                        sample.bones[bone][i]=previous->bones[bone][i]+alpha*(sample.bones[bone][i]-previous->bones[bone][i]);
                }
            }
            return sample;
        }
        void Reset(bool world=false) {
            std::lock_guard lock(mutex); peer.reset(); previous.reset(); assembling={}; chunks.fill(false); newest=generation=0;
            if(world) { local.reset(); localTime={}; }
        }
    };
}
