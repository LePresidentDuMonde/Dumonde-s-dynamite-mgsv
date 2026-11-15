#ifndef HOOK_DYNAMITESYNCIMPL_H
#define HOOK_DYNAMITESYNCIMPL_H

#include "BossQuietNextActionTaskActionCondition.h"
#include "DynamiteSyncSchema_generated.h"
#include "Tpp/BossQuietActionCommand.h"
#include "Tpp/BossQuietActionControllerImplWork.h"
#include "Tpp/BossQuietActionTask.h"
#include "Tpp/BossQuietDamage.h"
#include "Tpp/TppTypes.h"
#include "SyncPolicy.h"
#include "ReceiveDemux.h"

#include "BuddyScanState.h"
#include "DogReplicaState.h"

#include <atomic>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <thread>


namespace Dynamite {
    struct CarryDropInfo {
        uint32_t objectID;
        Vector3 pos;
        float rotY;
    };

    struct VehicleStateInfo {
        uint32_t objectID;
        Vector3 pos;
        float rotY;
        bool hasRot;
        bool driven;
    };

    struct VehicleSent {
        std::chrono::steady_clock::time_point time;
        VehicleStateInfo state;
    };

    struct QuestStateInfo {
        std::string name;
        std::string step;
        bool active;
        bool invoked;
        uint32_t faceID = 65536;
    };

    class DynamiteSyncImpl {
      public:
        DynamiteSyncImpl();
        ~DynamiteSyncImpl();

        void Init();
        void Stop();
        void SyncInit();
        void WaitForSync();
        bool IsSynchronized();

        [[deprecated("use SteamUDPSocket")]] void CreateGameSocket();
        [[deprecated("use SteamUDPSocket")]] void RemoveGameSocket();
        void *gameSocket = nullptr;
        [[deprecated("use SendRaw")]] bool Send(const flatbuffers::FlatBufferBuilder *builder) const;

        bool PartnerInSession() const;
        bool SendRaw(const flatbuffers::FlatBufferBuilder *builder) const;
        bool RecvRaw(void *buffer, int32_t size);
        void ProcessIncoming(); // called only by the Lua update, on the game thread
        void SetMissionContext(uint32_t mission, bool playing);
        void SendCoopHello();
        bool CoopReady() const;
        void SendQuestState(const QuestStateInfo &state);
        std::optional<QuestStateInfo> PopQuestState();
        BuddyScanState buddyScans;
        void SendBuddyScans();
        void UpdateBuddyScans(bool active);
        DogReplicaState dogReplica;
        void SendDogReplica(bool selected,uint32_t generation);
        flatbuffers::Offset<DynamiteMessage::MessageWrapper> WrapMessage(flatbuffers::FlatBufferBuilder &builder,
            uint32_t packet, DynamiteMessage::Message type, flatbuffers::Offset<void> message) const;

        void Ping();

        void RequestVar(const std::string &catName, const std::string &varName);
        void HandleRequestVar(const DynamiteMessage::MessageWrapper *w);

        void AddFixedUserMarker(const Vector3 *pos);
        static void HandleAddFixedUserMarker(const DynamiteMessage::MessageWrapper *w);

        void AddFollowUserMarker(const Vector3 *pos, uint32_t objectID);
        static void HandleAddFollowUserMarker(const DynamiteMessage::MessageWrapper *w);

        void RemoveUserMarker(uint32_t markerID);
        static void HandleRemoveUserMarker(const DynamiteMessage::MessageWrapper *w);

        void SetSightMarker(uint32_t objectID, float duration);
        static void HandleSetSightMarker(const DynamiteMessage::MessageWrapper *w);

        void SendEmblem();
        static void HandleSendEmblem(const DynamiteMessage::MessageWrapper *w);

        void SyncVar(const std::string &catName, const std::string &varName);
        void HandleSyncVar(const DynamiteMessage::MessageWrapper *w);

        // todo replace with template?
        static bool SyncBoolVar(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncInt32Var(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncUint32Var(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncInt16Var(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncUint16Var(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncInt8Var(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncUint8Var(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);
        static bool SyncFloatVar(const DynamiteMessage::SyncVar *m, void *vars, uint32_t varIndex);

        void SyncEnemyVars();

        void SendBossquietActionCommand(uint32_t p1, BossQuietActionCommand *command);
        static void HandleSendBossQuietActionCommand(const DynamiteMessage::MessageWrapper *w);

        void SendBossquietExtraActionCommand(uint32_t p1, BossQuietActionCommand *command);
        static void HandleSendBossQuietExtraActionCommand(const DynamiteMessage::MessageWrapper *w);

        void SendBossquietSetNextActionTask(
            uint32_t param_1, BossQuietActionTask *actionTask, BossQuietNextActionTaskActionCondition actionCondition);
        static void HandleSendBossquietSetNextActionTask(const DynamiteMessage::MessageWrapper *w);

        void SendBossquietSetActionTask(uint32_t param_1, BossQuietActionTask *task);
        static void HandleSendBossquietSetActionTask(const DynamiteMessage::MessageWrapper *w);

        void SendBossQuietDamage(BossQuietDamage *damage);
        void HandleSendBossQuietDamage(const DynamiteMessage::MessageWrapper *w);

        // npc put down by local player; partner's copy is moved by lua (PopCarryDrop), not on network thread
        void SendCarryDrop(uint32_t objectID, const Vector3 *pos, float rotY);
        void HandleCarryDrop(const DynamiteMessage::MessageWrapper *w);
        std::optional<CarryDropInfo> PopCarryDrop();

        // throttled per vehicle; incoming states keep only the latest one per vehicle, applied by lua (PopVehicleState)
        void SendVehicleState(const VehicleStateInfo &v);
        void HandleVehicleState(const DynamiteMessage::MessageWrapper *w);
        std::optional<VehicleStateInfo> PopVehicleState();

        void *steamUDPAddress = nullptr;
        void *steamUDPSocketInfo = nullptr;
        void *steamUDPSocket = nullptr;

        std::atomic<uint32_t> packetNumber = 0;
        std::mutex incomingMutex;
        std::deque<std::vector<uint8_t>> incoming;
        ReplayWindow replay;
        std::atomic<uint32_t> missionCode = 0;
        std::atomic<uint64_t> localEpoch = 0;
        std::atomic<uint64_t> peerEpoch = 0;
        std::atomic<uint64_t> pendingPeerEpoch = 0;
        std::recursive_mutex runtimeMutex;
        std::atomic<bool> missionPlaying = false;
        std::optional<QuestStateInfo> questState;
        uint32_t questSequence = 0;
        std::map<uint32_t, uint32_t> entitySequences;
        std::jthread syncThread;
        std::mutex carryDropsMutex;
        std::deque<CarryDropInfo> carryDrops;
        std::mutex vehicleStatesMutex;
        std::map<uint32_t, VehicleStateInfo> vehicleStates;
        std::map<uint32_t, VehicleSent> vehicleSent;
        std::map<std::string, bool> syncStatus{};
        std::map<std::string, std::vector<bool>> syncReceived;
        std::vector<std::string> enemyVars = {
            // "cpNames",
            // "cpFlags",
            // "solName",
            // "solState",
            // "solFlagAndStance",
            // "solWeapon",
            // "solLocation",
            // "solMarker",
            "solFovaSeed",
            "solFaceFova",
            "solBodyFova",
            // "solCp",
            // "solCpRoute",
            // "solScriptSneakRoute",
            // "solScriptCautionRoute",
            // "solScriptAlertRoute",
            // "solRouteNodeIndex",
            // "solRouteEventIndex",
            // "solTravelName",
            // "solTravelStepIndex",
            // "solOptName",
            // "solOptParam1",
            // "solOptParam2",
            // "passengerInfoName",
            // "passengerFlagName",
            // "passengerNameName",
            // "passengerVehicleNameName",
            // "noticeObjectType",
            // "noticeObjectPosition",
            // "noticeObjectOwnerName",
            // "noticeObjectOwnerId",
            // "noticeObjectAttachId",
            "solRandomSeed",
            // "hosName",
            // "hosState",
            // "hosFlagAndStance",
            // "hosWeapon",
            // "hosLocation",
            // "hosMarker",
            // "hosFovaSeed",
            // "hosFaceFova",
            // "hosBodyFova",
            // "hosScriptSneakRoute",
            // "hosRouteNodeIndex",
            // "hosRouteEventIndex",
            // "hosOptParam1",
            // "hosOptParam2",
            // "hosRandomSeed",
            // "enemyHeliName",
            // "enemyHeliLocation",
            // "enemyHeliCp",
            // "enemyHeliFlag",
            // "enemyHeliSneakRoute",
            // "enemyHeliCautionRoute",
            // "enemyHeliAlertRoute",
            // "enemyHeliRouteNodeIndex",
            // "enemyHeliRouteEventIndex",
            // "enemyHeliMarker",
            // "enemyHeliLife",
            // "ene_wkrg_name",
            // "ene_wkrg_life",
            // "ene_wkrg_partslife",
            // "ene_wkrg_location",
            // "ene_wkrg_bulletleft",
            // "ene_wkrg_marker",
            // "ene_holdRecoveredStateName",
            // "ene_isRecovered",
            // "ene_holdBrokenStateName",
            // "ene_isVehicleBroken",
            // "liquidLifeStatus",
            // "liquidMarker",
            // "uavName",
            // "uavIsDead",
            // "uavMarker",
            // "uavCp",
            // "uavPatrolRoute",
            // "uavCombatRoute",
            // "securityCameraCp",
            // "securityCameraMarker",
            // "securityCameraFlag",
        };
    };
}

#endif // HOOK_DYNAMITESYNCIMPL_H
