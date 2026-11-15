#pragma once

#include "SyncPolicy.h"
#include <chrono>
#include <mutex>
#include <set>
#include <vector>

namespace Dynamite {
    inline constexpr size_t MaxBuddyScans=64; // Stock Dog has 32 detection records.
    inline bool BuddyScanID(uint32_t id) {
        const auto type=id>>9;
        return id<0xffff && type>=2 && type!=13 && type!=18 && type!=19 && type!=20;
    }
    struct BuddyScanChange { uint16_t id; bool enabled; };
    // Separate local/remote contributions. The native reveal flag has just one bit.
    // Never hold this lock while calling the engine or transport (Dog jobs use worker threads).
    class BuddyScanState {
        std::mutex mutex;
        std::set<uint16_t> local, dog, peer, applied;
        uint32_t sequence=0;
        std::chrono::steady_clock::time_point received{};
      public:
        bool Observe(uint16_t id,bool enable,bool fromDog) {
            if(!BuddyScanID(id)) return false;
            std::lock_guard lock(mutex);
            if(enable) {
                if(local.size()<256 || local.contains(id)) local.insert(id);
                if(fromDog && (dog.size()<MaxBuddyScans || dog.contains(id))) dog.insert(id);
            } else { local.erase(id); dog.erase(id); }
            return !enable && peer.contains(id);
        }
        std::vector<uint16_t> Snapshot() {
            std::lock_guard lock(mutex); return {dog.begin(),dog.end()};
        }
        bool Receive(uint32_t seq,const std::vector<uint16_t> &ids,std::chrono::steady_clock::time_point now) {
            std::lock_guard lock(mutex);
            if(!NewerSequence(seq,sequence)) return false;
            sequence=seq; peer={ids.begin(),ids.end()}; received=now; return true;
        }
        std::vector<BuddyScanChange> Frame(bool active,std::chrono::steady_clock::time_point now) {
            std::lock_guard lock(mutex);
            if(!active || now-received>std::chrono::seconds(3)) peer.clear();
            std::vector<BuddyScanChange> out;
            for(auto id:applied) if(!peer.contains(id) && !local.contains(id)) out.push_back({id,false});
            // Refresh while streamed targets become available; replay never enters Observe.
            for(auto id:peer) out.push_back({id,true});
            applied=peer; return out;
        }
        void Reset(bool worldChanged=false) {
            std::lock_guard lock(mutex);
            peer.clear(); sequence=0; received={};
            if(worldChanged) { local.clear(); dog.clear(); applied.clear(); }
        }
    };
}
