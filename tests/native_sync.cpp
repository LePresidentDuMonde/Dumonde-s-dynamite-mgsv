// Runs the production receiver/senders with native entry points replaced by spies.
// No DLL injection, game process, Steam API or live GameObject is used.
#include "DynamiteHook.h"
#include "DynamiteSyncImpl/MessageValidation.h"
#include "ScriptVariableHandle.h"
#include "mgsvtpp_func_typedefs.h"
#include <bit>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using Mod = Dynamite::Dynamite;
using namespace DynamiteMessage;
void TestBuddyNative(Mod &host,Mod &guest);
void TestDogReplicaState();
namespace Dynamite::BuddyScans { extern float lastManualDuration; }
namespace {
    std::vector<uint8_t> wire;
    std::vector<std::vector<uint8_t>> captured;
    bool captureFrames=false;
    int sends = 0;
    bool failSend = false;
    uint32_t written = 0;
    int writes = 0;
    std::vector<uint32_t> worldValues(512, 42);
    uint16_t nativeCount = 1;
    uint8_t nativeType = TYPE_FLOAT;
    void *GetVars() { return reinterpret_cast<void *>(1); }
    uint32_t GetIndex(void *, uint32_t) { return 0; }
    uint64_t Hash(const char *, uint64_t) { return 1; }
    void *GetHandle(void *, void *result, uint32_t, uint32_t) {
        auto &handle = *static_cast<ScriptVariableHandle *>(result);
        handle.base = 1; handle.count = nativeCount; handle.flags = nativeType;
        return result;
    }
    void Check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
    int32_t Capture(void *, void *data, int size, void *, void *) {
        if (failSend) return -1;
        const auto bytes = static_cast<const uint8_t *>(data);
        wire.assign(bytes, bytes + size);
        if(captureFrames) captured.push_back(wire);
        ++sends;
        return size;
    }
    void SetVar(void *, uint64_t, uint32_t, uint32_t, uint32_t value) { written = value; ++writes; }
    void Transport(Mod &mod) {
        auto &sync = mod.dynamiteSyncImpl;
        sync.steamUDPSocket = sync.steamUDPAddress = reinterpret_cast<void *>(1);
        // Send at 0x141a5fb60 does not read its fourth argument. The engine passes null.
        sync.steamUDPSocketInfo = nullptr;
    }
    bool Deliver(Mod &mod, const std::vector<uint8_t> &packet, bool process = true) {
        Dynamite::g_hook = &mod;
        const auto ok = mod.dynamiteSyncImpl.RecvRaw(const_cast<uint8_t *>(packet.data()), packet.size());
        if (process) mod.dynamiteSyncImpl.ProcessIncoming();
        return ok;
    }
    void Handshake(Mod &host, Mod &guest) {
        Transport(host); Transport(guest);
        Dynamite::g_hook = &host;
        host.dynamiteSyncImpl.SendCoopHello();
        Check(Deliver(guest, wire), "host hello rejected");
        guest.dynamiteSyncImpl.SendCoopHello();
        Check(Deliver(host, wire), "guest hello rejected");
        host.dynamiteSyncImpl.SendCoopHello();
        Check(Deliver(guest, wire), "echo rejected");
        Check(guest.dynamiteSyncImpl.CoopReady(), "guest handshake incomplete");
        Dynamite::g_hook = &host;
        Check(host.dynamiteSyncImpl.CoopReady(), "host handshake incomplete");
    }
    template<class F> std::vector<uint8_t> Packet(Mod &sender, Mod &receiver, Message type, F create, uint32_t seq) {
        flatbuffers::FlatBufferBuilder b;
        const auto value = create(b);
        b.Finish(CreateMessageWrapper(b, seq, type, value.Union(), sender.dynamiteSyncImpl.missionCode,
            sender.dynamiteSyncImpl.localEpoch, receiver.dynamiteSyncImpl.localEpoch));
        std::vector<uint8_t> bytes(std::begin(DYNAMITE_RAW_HEADER), std::end(DYNAMITE_RAW_HEADER));
        bytes.insert(bytes.end(), b.GetBufferPointer(), b.GetBufferPointer() + b.GetSize());
        return bytes;
    }
}

// Only these facade functions are stubbed; protocol code under test is linked unchanged.
namespace Dynamite {
    Dynamite::Dynamite() = default;
    Dynamite::~Dynamite() = default;
    DynamiteCore::DynamiteCore() = default;
    int DynamiteCore::GetMemberCount() const { return 2; }
    void DynamiteCore::RemoveOpponentEmblemTexture() {}
    ::EmblemInfo DynamiteCore::GetEmblemInfo() { return {}; }
    void DynamiteCore::CreateEmblem(::EmblemInfo) {}
    ScriptVarResult DynamiteCore::GetSVar(const std::string &, const std::string &) {
        ScriptVarResult value{}; value.type = TYPE_UINT32; value.uint32s = worldValues; return value;
    }
    void DynamiteCore::BossQuietSetNextActionTask(uint32_t, BossQuietActionTask *, ::BossQuietNextActionTaskActionCondition) const {}
    void FoxNtImplGameSocketImplRequestToSendToMemberHook(void *, unsigned char, uint32_t, void *, uint32_t) {}
}
std::string bytes_to_hex(const void *, size_t) { return {}; }

int main() {
    try {
        spdlog::set_level(spdlog::level::off);
        FoxNioImplSteamUdpSocketImplSend = Capture;
        ScriptDeclVarsImplSetVarValue = SetVar;
        TppGmGetSVars = GetVars;
        TppGmImplScriptDeclVarsImplGetVarIndexWithName = GetIndex;
        FoxStrHash32 = Hash;
        TppGmImplScriptSystemImplGetScriptDeclVarHandle = GetHandle;
        Mod host, guest;
        host.cfg.Host = true;
        for (auto mod : { &host, &guest }) {
            Dynamite::g_hook = mod;
            mod->dynamiteSyncImpl.SetMissionContext(30010, false);
            mod->dynamiteSyncImpl.SetMissionContext(30010, true);
        }
        // Required endpoint data is still mandatory even though SocketInfo is optional.
        Dynamite::g_hook = &host;
        Transport(host);
        const auto beforeMissingEndpoint = sends;
        host.dynamiteSyncImpl.steamUDPAddress = nullptr;
        host.dynamiteSyncImpl.SendCoopHello();
        Check(sends == beforeMissingEndpoint, "sent without required destination address");
        Check(!host.dynamiteSyncImpl.SendRaw(nullptr), "accepted null packet builder");
        Handshake(host, guest);
        Dynamite::g_hook = &guest;
        guest.dynamiteSyncImpl.SendCoopHello();
        const auto oldHello = wire;
        Dynamite::g_hook = &host;
        host.dynamiteSyncImpl.SendQuestState({"test_quest", "Main", true, true, 123});
        const auto oldQuest = wire;
        Check(Deliver(guest, oldQuest, false), "quest receive failed");
        Check(!guest.dynamiteSyncImpl.PopQuestState(), "receive mutated gameplay before the game-thread drain");
        guest.dynamiteSyncImpl.ProcessIncoming();
        const auto quest = guest.dynamiteSyncImpl.PopQuestState();
        Check(quest && quest->name == "test_quest" && quest->faceID == 123, "quest roundtrip failed");
        Deliver(guest, oldQuest);
        Check(!guest.dynamiteSyncImpl.PopQuestState(), "duplicate quest applied");

        // Same mission code, new allocation: stale packets must never reach the new world.
        Dynamite::g_hook = &guest;
        guest.dynamiteSyncImpl.SetMissionContext(30010, false);
        guest.dynamiteSyncImpl.SetMissionContext(30010, true);
        Handshake(host, guest);
        const auto peerAfterRestart = host.dynamiteSyncImpl.peerEpoch.load();
        Deliver(host, oldHello);
        Check(host.dynamiteSyncImpl.peerEpoch == peerAfterRestart, "stale hello rolled the peer binding back");
        Deliver(guest, oldQuest);
        Check(!guest.dynamiteSyncImpl.PopQuestState(), "old allocation accepted after restart");

        auto vehicle = [&](uint32_t id, float x, uint32_t seq) {
            return Packet(host, guest, Message_VehicleState, [&](auto &b) {
                const Vec3 pos(x, 1, 2); return CreateVehicleState(b, id, &pos, 0, true, true);
            }, seq);
        };
        Deliver(guest, vehicle(50 * 512, 20, 100));
        Deliver(guest, vehicle(50 * 512, 10, 99));
        auto v = guest.dynamiteSyncImpl.PopVehicleState();
        Check(v && v->pos.x == 20, "reordered state undid a newer state");
        Deliver(guest, vehicle(50 * 512 + 1, 10, 98));
        Check(guest.dynamiteSyncImpl.PopVehicleState().has_value(), "unrelated reordered entity was lost");
        Check(!Deliver(guest, vehicle(0x10000, 1, 101)), "virtual horse ID was accepted as a live vehicle");
        Check(!Deliver(guest, vehicle(50 * 512, std::numeric_limits<float>::quiet_NaN(), 102)), "NaN accepted");
        Check(!Deliver(guest, vehicle(50 * 512, 100000, 103)), "out-of-world position accepted");

        const auto malformed = Packet(host, guest, Message_RequestVar, [](auto &b) { return CreateRequestVar(b); }, 104);
        Check(!Deliver(guest, malformed), "missing variable names accepted");
        const auto shortBoss = Packet(host, guest, Message_SendBossQuietActionCommand, [](auto &b) {
            return CreateSendBossQuietActionCommand(b, 0, b.CreateVector(std::vector<int8_t>{1}));
        }, 105);
        Check(!Deliver(guest, shortBoss), "short native blob accepted");
        const auto saveWrite = Packet(host, guest, Message_RequestVar, [](auto &b) {
            const auto category = b.CreateString("gvars"); const auto name = b.CreateString("qst_questClearedFlag");
            return CreateRequestVar(b, category, name);
        }, 106);
        Check(!Deliver(guest, saveWrite), "save/progression variable accepted");
        const auto unauthorizedQuest = Packet(guest, host, Message_QuestState, [](auto &b) {
            const auto name=b.CreateString("guest_quest"), step=b.CreateString("Main");
            return CreateQuestState(b, name, step, true, true);
        }, 107);
        Deliver(host, unauthorizedQuest);
        Check(!host.dynamiteSyncImpl.PopQuestState(), "guest changed the authoritative quest");

        // The real float applicator must pass the float's bits to the native integer ABI.
        flatbuffers::FlatBufferBuilder floats;
        const auto category = floats.CreateString("svars"), name = floats.CreateString("solFovaSeed");
        const auto array = CreateFloat(floats, floats.CreateVector(std::vector<float>{1.5f}));
        const auto variable = CreateSyncVar(floats, category, name, 1, 0, ArrayValues_Float, array.Union());
        floats.Finish(variable);
        Dynamite::DynamiteSyncImpl::SyncFloatVar(flatbuffers::GetRoot<SyncVar>(floats.GetBufferPointer()), nullptr, 0);
        Check(writes == 1 && written == std::bit_cast<uint32_t>(1.5f), "float ABI corrupted");
        const auto floatPacket = Packet(host, guest, Message_SyncVar, [](auto &b) {
            const auto cat=b.CreateString("svars"), name=b.CreateString("solFovaSeed");
            const auto a=CreateFloat(b, b.CreateVector(std::vector<float>{1.5f}));
            return CreateSyncVar(b, cat, name, 1, 0, ArrayValues_Float, a.Union());
        }, 110);
        nativeType=TYPE_UINT32;
        Deliver(guest, floatPacket);
        Check(writes==1, "received type mismatch reached native setter");
        const auto oversizedArray = Packet(host, guest, Message_SyncVar, [](auto &b) {
            const auto cat=b.CreateString("svars"), name=b.CreateString("solFovaSeed");
            const auto a=CreateUint32(b, b.CreateVector(std::vector<uint32_t>{1,2}));
            return CreateSyncVar(b, cat, name, 1, 0, ArrayValues_Uint32, a.Union());
        }, 111);
        Check(!Deliver(guest, oversizedArray) && writes==1, "out-of-bounds array reached setter");

        // Driven vehicles need heartbeats even when motionless; failed sends cannot update the throttle cache.
        Dynamite::g_hook = &host;
        auto &sync = host.dynamiteSyncImpl;
        Dynamite::VehicleStateInfo stationary{50 * 512, {.x=1,.y=2,.z=3}, 0, true, true};
        sync.SendVehicleState(stationary);
        const auto count = sends;
        sync.vehicleSent[stationary.objectID].time -= std::chrono::milliseconds(600);
        sync.SendVehicleState(stationary);
        Check(sends == count + 1, "stationary driver heartbeat missing");
        failSend = true;
        stationary.objectID++;
        sync.SendVehicleState(stationary);
        Check(!sync.vehicleSent.contains(stationary.objectID), "failed send cached as successful");
        failSend = false;
        const auto chunksBefore = sends;
        sync.SyncVar("svars", "solFovaSeed");
        Check(sends == chunksBefore + 4 && wire.size() <= Dynamite::MaxSyncPayload + 8, "512-element variable was not safely fragmented");

        Dynamite::ReplayWindow window;
        Check(window.Accept(0xfffffffe) && window.Accept(0xffffffff) && window.Accept(1) && !window.Accept(0xffffffff), "sequence wrap/replay handling failed");
        // Truncation, corrupted offsets, empty and oversized frames must fail without native calls.
        Dynamite::g_hook = &guest;
        for (size_t length = 0; length < oldQuest.size(); ++length) {
            guest.dynamiteSyncImpl.RecvRaw(const_cast<uint8_t *>(oldQuest.data()), length);
        }
        std::mt19937 rng(42);
        for (int i=0; i<10000; ++i) {
            std::vector<uint8_t> fuzz(rng() % 1100);
            for (auto &byte : fuzz) byte = rng();
            if (fuzz.size() >= 8) std::memcpy(fuzz.data(), DYNAMITE_RAW_HEADER, 8);
            Check(!guest.dynamiteSyncImpl.RecvRaw(fuzz.data(), fuzz.size()), "random frame accepted");
        }
        Check(guest.dynamiteSyncImpl.incoming.size() <= 128, "receive queue unbounded");
        // Use a new mission allocation so these real senders have a fresh replay window.
        for(auto mod:{&host,&guest}) { Dynamite::g_hook=mod; mod->dynamiteSyncImpl.SetMissionContext(30010,false); mod->dynamiteSyncImpl.SetMissionContext(30010,true); }
        Handshake(host,guest);
        auto &hs=host.dynamiteSyncImpl; auto &gs=guest.dynamiteSyncImpl;
        hs.buddyScans.Observe(1024,true,true);
        Dynamite::g_hook=&host; hs.SendBuddyScans(); const auto firstScan=wire;
        Check(Deliver(guest,wire,false),"host Dog scans rejected");
        Check(gs.buddyScans.Frame(true,std::chrono::steady_clock::now()).empty(),"network thread applied scan state");
        gs.ProcessIncoming();
        auto scans=gs.buddyScans.Frame(true,std::chrono::steady_clock::now());
        Check(scans.size()==1 && scans[0].id==1024 && scans[0].enabled,"host Dog scan not visible to guest");
        Check(gs.buddyScans.Snapshot().empty(),"remote Dog scan echoed back as a local detection");
        gs.buddyScans.Observe(2048,true,true);
        Dynamite::g_hook=&guest; gs.SendBuddyScans();
        Check(Deliver(host,wire),"guest Dog scans rejected");
        scans=hs.buddyScans.Frame(true,std::chrono::steady_clock::now());
        Check(scans.size()==1 && scans[0].id==2048 && hs.buddyScans.Snapshot()==std::vector<uint16_t>{1024},"guest Dog scan not visible to host, or own detection overwritten");
        hs.buddyScans.Observe(1024,false,true); hs.buddyScans.Observe(1025,true,true);
        Dynamite::g_hook=&host; hs.SendBuddyScans(); Deliver(guest,wire); Deliver(guest,firstScan);
        scans=gs.buddyScans.Frame(true,std::chrono::steady_clock::now());
        Check(scans.size()==2 && scans[0].id==1024 && !scans[0].enabled && scans[1].id==1025 && scans[1].enabled,"reordered scan snapshot restored an old detection");
        hs.buddyScans.Observe(1025,false,true);
        Dynamite::g_hook=&host; hs.SendBuddyScans(); // packet deliberately lost
        hs.SendBuddyScans(); Check(Deliver(guest,wire),"repeated complete scan snapshot rejected");
        scans=gs.buddyScans.Frame(true,std::chrono::steady_clock::now());
        Check(scans.size()==1 && scans[0].id==1025 && !scans[0].enabled,"lost disappearance never recovered");
        const auto badScans=[&](std::vector<uint16_t> ids) {
            return Packet(host,guest,Message_BuddyScans,[&](auto &b){ return CreateBuddyScans(b,b.CreateVector(ids)); },++hs.packetNumber);
        };
        Check(!Deliver(guest,badScans({1024,1024})),"duplicate scan IDs accepted");
        Check(!Deliver(guest,badScans({1025,1024})),"unsorted scan IDs accepted");
        Check(!Deliver(guest,badScans({1})) && !Deliver(guest,badScans({19*512})),"player/companion accepted as scan target");
        std::vector<uint16_t> oversized; for(uint16_t id=1024;id<1089;++id) oversized.push_back(id);
        Check(!Deliver(guest,badScans(oversized)),"oversized scan vector accepted");
        const auto missingScans=Packet(host,guest,Message_BuddyScans,[](auto &b){ return CreateBuddyScans(b); },++hs.packetNumber);
        Check(!Deliver(guest,missingScans),"missing scan vector accepted");
        const auto legacy=Packet(host,guest,Message_BuddyState,[](auto &b){ return CreateBuddyState(b,1,2); },++hs.packetNumber);
        Check(!Deliver(guest,legacy),"retired shared-companion loadout accepted");
        const auto oldProtocol=Packet(host,guest,Message_CoopHello,[](auto &b){ return CreateCoopHello(b,2,true); },++hs.packetNumber);
        Check(!Deliver(guest,oldProtocol),"mixed shared/personal companion protocol accepted");
        const auto badDuration=Packet(host,guest,Message_SetSightMarker,[](auto &b){ return CreateSetSightMarker(b,1024,std::numeric_limits<float>::quiet_NaN()); },++hs.packetNumber);
        Check(!Deliver(guest,badDuration),"non-finite marker duration accepted");
        const auto fractional=Packet(host,guest,Message_SetSightMarker,[](auto &b){ return CreateSetSightMarker(b,1024,1.25f); },++hs.packetNumber);
        Check(Deliver(guest,fractional),"fractional manual marker duration rejected");
        Check(Dynamite::BuddyScans::lastManualDuration==1.25f,"fractional marker duration truncated in production receiver");
        Dynamite::g_hook=&guest; gs.UpdateBuddyScans(false);
        gs.SetMissionContext(30010,false); gs.SetMissionContext(30010,true); Handshake(host,guest);
        Check(gs.buddyScans.Snapshot()==std::vector<uint16_t>{2048},"same-world reconnect lost local Dog's existing detections");
        gs.buddyScans.Reset(true); // actual Lua mission reload boundary
        Deliver(guest,firstScan);
        Check(gs.buddyScans.Frame(true,std::chrono::steady_clock::now()).empty(),"old world scan reached replacement actors");
        TestBuddyNative(host,guest);
        TestDogReplicaState();
        {
            Dynamite::DogMatrix identity{};identity[0]=identity[5]=identity[10]=identity[15]=1;
            Dynamite::DogPose dog;dog.active=true;dog.generation=7;dog.world=identity;dog.world[12]=6;dog.position={17,2,3};dog.bones.assign(13,identity);
            const auto now=std::chrono::steady_clock::now();
            hs.dogReplica.Capture(dog,now);
            Dynamite::g_hook=&host;captureFrames=true;captured.clear();hs.SendDogReplica(true,7);captureFrames=false;
            auto hostPose=captured;
            Check(hostPose.size()==2,"real skeletal sender failed to fragment bones into bounded packets");
            for(const auto &part:hostPose) Check(part.size()<=Dynamite::MaxSyncPayload+sizeof(DYNAMITE_RAW_HEADER),"skeletal packet exceeds engine UDP envelope");
            Check(Deliver(guest,hostPose[1]) && !gs.dogReplica.Peer(now),"network receiver committed a partial skeleton");
            Check(Deliver(guest,hostPose[0]),"reordered skeletal chunk rejected");
            auto replica=gs.dogReplica.Peer(std::chrono::steady_clock::now());
            Check(replica && replica->owner==0 && replica->bones.size()==13 && replica->world[12]==6 && replica->position[0]==17 && replica->rotation[3]==1,"host Dog was not replicated to the guest");
            dog.bones.resize(8);dog.world[12]=9;gs.dogReplica.Capture(dog,now);
            Dynamite::g_hook=&guest;gs.SendDogReplica(true,7);Check(Deliver(host,wire),"guest Dog packet rejected");
            auto other=hs.dogReplica.Peer(std::chrono::steady_clock::now());
            Check(other && other->owner==1 && other->bones.size()==8 && other->world[12]==9,"guest Dog replaced host Dog or lost canonical ownership");
            const auto spoof=Packet(guest,host,Message_DogReplicaPose,[](auto &b){
                const auto empty=b.CreateVector(std::vector<float>{});return CreateDogReplicaPose(b,7,200,0,false,0,0,0,empty,empty);
            },++gs.packetNumber);
            Deliver(host,spoof);Check(hs.dogReplica.Peer(now).has_value(),"guest pose could remove host's own actor identity");
            Dynamite::g_hook=&host;hs.SendDogReplica(false,7);Deliver(guest,wire);
            Check(!gs.dogReplica.Peer(now),"unequipped host Dog left a remote duplicate");
            for(const auto &part:hostPose) Deliver(guest,part);
            Check(!gs.dogReplica.Peer(now),"late fragments resurrected a recalled Dog");
            dog.bones.assign(Dynamite::MaxDogBones,identity);hs.dogReplica.Capture(dog,now);
            Dynamite::g_hook=&host;captureFrames=true;captured.clear();hs.SendDogReplica(true,7);captureFrames=false;
            Check(captured.size()==(Dynamite::MaxDogBones+Dynamite::DogBonesPerChunk-1)/Dynamite::DogBonesPerChunk,"maximum bounded skeleton was truncated by payload limits");
            for(const auto &part:captured) Check(Deliver(guest,part),"valid maximum-size skeletal frame rejected");
            Check(gs.dogReplica.Peer(std::chrono::steady_clock::now())->bones.size()==Dynamite::MaxDogBones,"maximum skeleton reassembly failed");
            const auto incompatible=Packet(host,guest,Message_CoopHello,[](auto &b){return CreateCoopHello(b,3,true);},++hs.packetNumber);
            Check(!Deliver(guest,incompatible),"protocol 3 scan-only client joined a two-Dog session");
            Dynamite::g_hook=&guest;gs.SetMissionContext(30010,false);gs.SetMissionContext(30010,true);Handshake(host,guest);
            for(const auto &part:captured) Deliver(guest,part);
            Check(!gs.dogReplica.Peer(now),"old-world skeleton animated newly loaded Dog actors");
        }
        std::cout << "PASS: production protocol, restart epochs, game-thread drain, ordering, validation, floats, throttling, fragmentation, 10000 invalid frames\n";
        std::cout << "PASS: native transport with null optional SocketInfo; missing destination still rejected\n";
        std::cout << "PASS: independent companions, bidirectional Dog scans, overlap, disappearance, loss, reordering, old worlds, protocol isolation\n";
        std::cout << "PASS: two-Dog skeletal frames, canonical owners, bounded fragments, interpolation, native matrix bounds, recall, generation and epoch isolation\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
