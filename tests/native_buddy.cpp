#include "DynamiteHook.h"
#include "BuddyOwnership.h"
#include "BuddyScanMessage.h"
#include <stdexcept>
#include <thread>

// Native applicator spies. The actual send/receive and merge policy are linked unchanged.
namespace Dynamite::BuddyScans {
    int applications=0;
    float lastManualDuration=-1;
    bool Apply(uint16_t,bool) { ++applications; return true; }
    bool ApplyManual(uint16_t,float duration) { lastManualDuration=duration; return true; }
}
void TestBuddyNative(Dynamite::Dynamite &,Dynamite::Dynamite &) {
    using namespace Dynamite;
    auto check=[](bool v,const char *s) { if(!v) throw std::runtime_error(s); };
    check(PersonalBuddyOwner(true,false)==0 && PersonalBuddyOwner(false,false)==0,"solo buddy owner changed");
    check(PersonalBuddyOwner(true,true)==0 && PersonalBuddyOwner(false,true)==1,"personal buddy owners collide");
    alignas(16) uint8_t command[32]{},local[32]{};
    uint64_t attack=0x12b9a4e6da92, pointer=0x123456789abcdef;
    uint16_t target=1024,player=0;
    std::memcpy(command,&attack,8); std::memcpy(command+8,&target,2); std::memcpy(command+10,&player,2);
    std::memcpy(command+16,&pointer,8); command[24]=1;
    const auto original=std::vector<uint8_t>(command,command+32);
    auto rebound=PersonalDogCommand(reinterpret_cast<const uint64_t *>(command),1,local);
    check(reinterpret_cast<const uint8_t *>(rebound)[10]==1 && reinterpret_cast<const uint8_t *>(rebound)[24]==1,"guest order owner/option lost");
    check(std::memcmp(local+8,command+8,2)==0 && std::memcmp(local+16,&pointer,8)==0,"guest order lost its native target or local pointer");
    check(std::memcmp(command,original.data(),32)==0,"guest order mutated engine input");
    attack=0xdeadbeef; std::memcpy(command,&attack,8);
    check(PersonalDogCommand(reinterpret_cast<const uint64_t *>(command),1,local)==reinterpret_cast<const uint64_t *>(command),"unrelated native order rewritten");
    BuddyScanState state;
    const auto now=std::chrono::steady_clock::now();
    BuddyRevealPayload native; native.flags=1;
    check(!MergeLocalBuddyReveal(state,1024,&native) && state.Snapshot()==std::vector<uint16_t>{1024},"native Dog reveal was not captured");
    native.hash=ManualMarkerMessage;
    MergeLocalBuddyReveal(state,1025,&native);
    check(state.Snapshot().size()==1,"manual mark became a Dog scan");
    native.hash=BuddyRevealMessage; native.reason=0;
    MergeLocalBuddyReveal(state,1025,&native); // local reveal from another engine source
    check(state.Snapshot().size()==1,"non-Dog reveal was broadcast");
    check(state.Receive(1,{1024,1025,1026},now),"peer scan snapshot rejected");
    check(state.Frame(true,now).size()==3,"remote scans not applied");
    native.flags=0;
    check(MergeLocalBuddyReveal(state,1024,&native),"local D-Dog hid a target still detected by peer");
    check(state.Snapshot().empty(),"local Dog disappearance not transmitted");
    check(!state.Receive(1,{},now) && !state.Receive(0xffffffff,{},now),"old scan packet removed newer detections");
    auto expired=state.Frame(true,now+std::chrono::seconds(4));
    check(expired.size()==2 && !expired[0].enabled && expired[0].id==1024 && expired[1].id==1026,"timeout removed a local reveal or kept remote-only scans");
    check(state.Frame(true,now+std::chrono::seconds(5)).empty(),"timeout repeatedly cleared targets");
    state.Receive(2,{1025,1027},now);
    state.Frame(true,now);
    auto disconnected=state.Frame(false,now);
    check(disconnected.size()==1 && disconnected[0].id==1027 && !disconnected[0].enabled,"disconnect erased own detection");
    state.Reset(true);
    check(state.Snapshot().empty() && state.Frame(true,now).empty(),"new world retained marker IDs");
    for(uint16_t id=1024;id<1200;++id) state.Observe(id,true,true);
    check(state.Snapshot().size()==MaxBuddyScans,"scan capture unbounded");
    std::jthread worker([&]{ for(int i=0;i<5000;++i) state.Observe(1024,i&1,true); });
    for(int i=0;i<5000;++i) { state.Snapshot(); state.Frame(false,now); }
}
