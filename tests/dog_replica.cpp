#include "DynamiteSyncImpl/DogReplicaState.h"
#include <limits>
#include <stdexcept>

void TestDogReplicaState() {
    using namespace Dynamite;
    auto check=[](bool value,const char *s){if(!value) throw std::runtime_error(s);};
    check(DogPhysicalSlot({0,0xffff},1,0)==0 && !DogPhysicalSlot({0,0xffff},1,1),"local capture waited for a remote model that had not yet been realized");
    check(DogPhysicalSlot({1,0},2,0)==1 && DogPhysicalSlot({1,0},2,1)==0,"reassigned engine slots exchanged personal Dog identities");
    check(!DogPhysicalSlot({0,0},2,0) && !DogPhysicalSlot({0,1},3,0),"ambiguous or out-of-bounds native slot mapping accepted");
    DogMatrix identity{}; identity[0]=identity[5]=identity[10]=identity[15]=1;
    DogPosePart a; a.pose.active=true; a.pose.frame=1; a.pose.owner=1; a.pose.world=identity;
    a.boneCount=13; a.pose.bones.assign(DogBonesPerChunk,identity);
    auto b=a; b.firstBone=DogBonesPerChunk; b.pose.bones.resize(13-DogBonesPerChunk);
    const auto now=std::chrono::steady_clock::now();
    DogReplicaState state;
    check(!state.Receive(b,now) && !state.Peer(now),"partial skeleton became visible");
    check(state.Receive(a,now),"out-of-order skeletal chunks did not assemble");
    auto complete=state.Peer(now);
    check(complete && complete->bones.size()==13,"bone count lost during reassembly");
    std::array<DogMatrix,15> destination{}; destination.front().fill(123); destination.back().fill(456);
    check(CopyDogMatrices(destination.data()+1,13,0,*complete),"native matrix copy rejected a complete matching skeleton");
    check(destination.front()[0]==123 && destination.back()[15]==456,"native skeletal write crossed its allocated bone buffer");
    check(!CopyDogMatrices(destination.data(),12,0,*complete) && destination.front()[0]==123,"mismatched native skeleton was overwritten");
    check(!CopyDogMatrices(destination.data(),13,1,*complete),"wrong native coordinate space was overwritten");
    check(!state.Receive(a,now) && !state.Receive(b,now),"duplicate pose applied twice");
    a.pose.frame=b.pose.frame=2;
    check(!state.Receive(a,now),"incomplete second pose committed"); // lose part b
    check(state.Peer(now)->frame==1,"packet loss replaced a complete pose with partial data");
    a.pose.frame=b.pose.frame=3; a.pose.world[12]=b.pose.world[12]=10;
    state.Receive(a,now); check(state.Receive(b,now),"next complete pose did not recover lost chunk");
    auto halfway=state.Peer(now+std::chrono::milliseconds(25));
    check(halfway && halfway->world[12]==5,"complete poses were not interpolated");
    check(!state.Peer(now+std::chrono::seconds(4)),"disconnected Dog model stayed active indefinitely");
    auto invalid=a; invalid.pose.frame=10; invalid.pose.bones[0][0]=std::numeric_limits<float>::quiet_NaN();
    check(!state.Receive(invalid,now),"non-finite joint reached a native pose");
    invalid=a; invalid.boneCount=MaxDogBones+1; check(!state.Receive(invalid,now),"unbounded skeleton accepted");
    invalid=a; invalid.firstBone=1; check(!state.Receive(invalid,now),"unaligned bone fragment accepted");
    invalid=a; invalid.pose.world.fill(0);check(!state.Receive(invalid,now),"singular world matrix accepted");
    invalid=a; invalid.pose.frame=10; invalid.pose.rotation.fill(0);check(!state.Receive(invalid,now),"invalid actor quaternion accepted");
    invalid=a; invalid.pose.frame=10; invalid.pose.position[0]=std::numeric_limits<float>::infinity();check(!state.Receive(invalid,now),"invalid actor position accepted");
    DogPosePart gone; gone.pose.owner=1; gone.pose.frame=4;
    check(state.Receive(gone,now) && !state.Peer(now),"companion recall left a duplicate actor visible");
    check(!state.Receive(b,now),"late animation resurrected a recalled Dog");
    a.pose.generation=b.pose.generation=2; a.pose.frame=b.pose.frame=5;
    state.Receive(a,now);state.Receive(b,now);
    auto stale=a;stale.pose.generation=1;stale.pose.frame=6;
    check(!state.Receive(stale,now),"old Dog allocation animated its replacement");
    DogPose own=a.pose;own.bones.assign(13,identity);state.Capture(own,now);
    check(state.Outgoing(true,0,2,now).active,"own Dog pose not published");
    check(!state.Outgoing(true,0,3,now).active,"old local allocation published after replacement");
    check(!state.Outgoing(false,0,2,now).active,"unequipped Dog published as active");
    state.Reset();
    check(!state.Peer(now) && state.Outgoing(true,0,2,now).active,"reconnect lost local pose or kept remote Dog");
    state.Reset(true);
    check(!state.Outgoing(true,0,2,now).active,"new world retained a native pose");
}
