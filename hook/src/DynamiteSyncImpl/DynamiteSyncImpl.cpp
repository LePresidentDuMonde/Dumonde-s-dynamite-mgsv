#include "DynamiteSyncImpl.h"

#include "BossQuietNextActionTaskActionCondition.h"
#include "DynamiteHook.h"
#include "DynamiteSyncSchema_generated.h"
#include "EmblemInfo.h"
#include "Tpp/BossQuietDamage.h"
#include "flatbuffers/flatbuffers.h"
#include "memtag.h"
#include <array>
#include "mgsvtpp_func_typedefs.h"
#include "spdlog/spdlog.h"
#include "util.h"
#include "MessageValidation.h"
#include "ScriptVariableHandle.h"
#include "BuddyScans.h"

#include <array>
#include <bit>
#include <ranges>
#include <vector>

namespace Dynamite {
    DynamiteSyncImpl::DynamiteSyncImpl() = default;

    DynamiteSyncImpl::~DynamiteSyncImpl() {
        Stop();
        if (this->gameSocket != nullptr) {
            FoxNtImplGameSocketImplGameSocketImplDtor(this->gameSocket, 1);
        }
    }

    void DynamiteSyncImpl::SyncInit() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        syncStatus.clear();
        syncReceived.clear();

        for (const auto &v : enemyVars) {
            spdlog::info("{}, {}", __PRETTY_FUNCTION__, v);
            syncStatus[v] = false;
        }
    }

    void DynamiteSyncImpl::Init() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        std::lock_guard lock(runtimeMutex);
        static std::atomic<uint64_t> epochs{static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
        localEpoch = epochs.fetch_add(1) + 1;
        peerEpoch = 0;
        pendingPeerEpoch = 0;
        replay = {};
        questSequence = 0;
        questState.reset();
        buddyScans.Reset();
        dogReplica.Reset();
        entitySequences.clear();
        { std::lock_guard queueLock(incomingMutex); incoming.clear(); }
        { std::lock_guard queueLock(carryDropsMutex); carryDrops.clear(); }
        { std::lock_guard queueLock(vehicleStatesMutex); vehicleStates.clear(); vehicleSent.clear(); }
    }

    void DynamiteSyncImpl::Stop() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        if (syncThread.joinable()) {
            syncThread.request_stop();
            syncThread.join();
        }
        spdlog::info("{}, sync thread {}", __PRETTY_FUNCTION__, syncThread.joinable());

        Init();

        steamUDPSocket = nullptr;
        steamUDPAddress = nullptr;
        steamUDPSocketInfo = nullptr;

        {
            std::lock_guard lock(carryDropsMutex);
            carryDrops.clear();
        }

        {
            std::lock_guard lock(vehicleStatesMutex);
            vehicleStates.clear();
            vehicleSent.clear();
        }

        g_hook->dynamiteCore.RemoveOpponentEmblemTexture();
    }

    void DynamiteSyncImpl::RemoveGameSocket() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        if (this->gameSocket != nullptr) {
            spdlog::info("{}, removing socket", __PRETTY_FUNCTION__);
            FoxNtImplGameSocketImplGameSocketImplDtor(this->gameSocket, 1);
            this->gameSocket = nullptr;
        } else {
            spdlog::info("{}, no socket", __PRETTY_FUNCTION__);
        }
    }

    bool DynamiteSyncImpl::IsSynchronized() {
        if (std::ranges::all_of(syncStatus, [](const auto &pair) { return pair.second; })) {
            spdlog::info("{}, sync done", __PRETTY_FUNCTION__);
            return true;
        }

        return false;
    }

    void DynamiteSyncImpl::CreateGameSocket() {
        auto qt = GetQuarkSystemTable();
        // auto a = *(void **)((char *)qt + 0x98);
        // auto statusController = *(void **)((char *)a + 0xf8);
        // if (!StatusControllerImplIsSet(statusController, S_IS_ONLINE)) {
        //     spdlog::info("{}, no online flag", __PRETTY_FUNCTION__);
        //     return;
        // }

        const auto networkSystemImpl = *(void **)((char *)qt + 0x78);
        auto desc = fox::nt::GameSocketDesc{
            .socketNumber = 90,
            .value = 1,
        };

        if (this->gameSocket == nullptr) {
            this->gameSocket = FoxNtImplNetworkSystemImplCreateGameSocket(networkSystemImpl, &desc);
            spdlog::info("{}, socket {}", __PRETTY_FUNCTION__, this->gameSocket);
            FoxNtImplGameSocketImplSetInterval(this->gameSocket, 90, 0, 0);
            FoxNtImplGameSocketImplSetInterval(this->gameSocket, 90, 1, 0);
        }
    }

    void DynamiteSyncImpl::WaitForSync() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        if (syncStatus.size() == 0) {
            spdlog::info("{} nothing to wait for", __PRETTY_FUNCTION__);
            return;
        }

        if (syncThread.joinable()) {
            syncThread.request_stop();
            syncThread.join();
        }

        syncThread = std::jthread([this](const std::stop_token &stoken) {
            spdlog::info("{}, starting sync thread", __PRETTY_FUNCTION__);
            while (!stoken.stop_requested()) {
                for (const auto &[key, value] : syncStatus) {
                    if (value) {
                        continue;
                    }

                    RequestVar("svars", key);
                }

                if (IsSynchronized()) {
                    break;
                }

                using namespace std::chrono_literals;
                std::this_thread::sleep_for(500ms);
            }

            spdlog::info("{}, stopping sync thread", __PRETTY_FUNCTION__);
        });
    }

    void DynamiteSyncImpl::Ping() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(512);
        const auto sv = DynamiteMessage::CreatePing(builder, 1);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_Ping, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, res {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::RequestVar(const std::string &catName, const std::string &varName) {
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(512);
        const flatbuffers::Offset<flatbuffers::String> category = builder.CreateString(catName);
        const flatbuffers::Offset<flatbuffers::String> name = builder.CreateString(varName);
        const auto sv = DynamiteMessage::CreateRequestVar(builder, category, name);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_RequestVar, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, request for {}.{}, res {}", __PRETTY_FUNCTION__, catName, varName, res);
    }

    void DynamiteSyncImpl::HandleRequestVar(const DynamiteMessage::MessageWrapper *w) {
        const auto m = w->msg_as_RequestVar();
        SyncVar(m->category()->c_str(), m->name()->c_str());
    }

    void DynamiteSyncImpl::HandleAddFixedUserMarker(const DynamiteMessage::MessageWrapper *w) {
        if (hookState.markerSystemImpl == nullptr) {
            spdlog::error("{}, marker system is null!", __PRETTY_FUNCTION__);
            return;
        }

        const auto m = w->msg_as_AddFixedUserMarker();
        const auto pos = m->pos();
        spdlog::info("{}: x: {:f}, y: {:f}, z: {:f}", __PRETTY_FUNCTION__, pos->x(), pos->y(), pos->z());
        auto vv = Vector3{
            .x = pos->x(),
            .y = pos->y(),
            .z = pos->z(),
        };

        Marker2SystemImplPlacedUserMarkerFixed(hookState.markerSystemImpl, &vv);
    }

    void DynamiteSyncImpl::AddFollowUserMarker(const Vector3 *pos, const uint32_t objectID) {
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128);
        const auto p = DynamiteMessage::Vec3(pos->x, pos->y, pos->z);
        const auto sv = DynamiteMessage::CreateAddFollowUserMarker(builder, &p, objectID);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_AddFollowUserMarker, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}: {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleAddFollowUserMarker(const DynamiteMessage::MessageWrapper *w) {
        if (hookState.markerSystemImpl == nullptr) {
            spdlog::error("{}, marker system is null!", __PRETTY_FUNCTION__);

            return;
        }

        const auto m = w->msg_as_AddFollowUserMarker();
        auto vv = Vector3{
            .x = m->pos()->x(),
            .y = m->pos()->y(),
            .z = m->pos()->z(),
        };

        spdlog::info("{}: x: {}, y: {}, z: {}, objectID: {}", __PRETTY_FUNCTION__, m->pos()->x(), m->pos()->y(), m->pos()->z(), m->object_id());
        auto ok = Marker2SystemImplPlacedUserMarkerFollow(hookState.markerSystemImpl, &vv, m->object_id());
        spdlog::info("{}: {}", __PRETTY_FUNCTION__, ok);
    }

    void DynamiteSyncImpl::RemoveUserMarker(const uint32_t markerID) {
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128);
        const auto sv = DynamiteMessage::CreateRemoveUserMarker(builder, markerID);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_RemoveUserMarker, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}: {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleRemoveUserMarker(const DynamiteMessage::MessageWrapper *w) {
        if (hookState.markerSystemImpl == nullptr) {
            spdlog::error("{}, marker system is null!", __PRETTY_FUNCTION__);

            return;
        }

        const auto m = w->msg_as_RemoveUserMarker();
        spdlog::info("{}, id {}", __PRETTY_FUNCTION__, m->marker_id());
        Marker2SystemImplRemovedUserMarker(hookState.markerSystemImpl, m->marker_id());
    }

    void DynamiteSyncImpl::SetSightMarker(const uint32_t objectID, const float duration) {
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128);
        const auto sv = DynamiteMessage::CreateSetSightMarker(builder, objectID, duration);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SetSightMarker, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}: {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSetSightMarker(const DynamiteMessage::MessageWrapper *w) {
        const auto m = w->msg_as_SetSightMarker();
        // The old cached SightManager belonged to whichever player initialized last.
        // The object dispatcher addresses the current world directly, without replaying marking rewards.
        BuddyScans::ApplyManual(m->object_id(),m->duration());
    }

    void DynamiteSyncImpl::SendEmblem() {
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(256);

        const auto info = g_hook->dynamiteCore.GetEmblemInfo();

        const DynamiteMessage::EmblemInfo emblem_info{
            info.emblemTextureTag,
            info.emblemColorL,
            info.emblemColorH,
            info.emblemX,
            info.emblemY,
            info.emblemScale,
            info.emblemRotate,
        };

        const auto sv = DynamiteMessage::CreateSendEmblem(builder, &emblem_info);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SendEmblem, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}: {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSendEmblem(const DynamiteMessage::MessageWrapper *w) {
        const auto m = w->msg_as_SendEmblem();
        const auto info = m->emblem_info();
        if (info == nullptr) {
            spdlog::error("{}, emblem info is nullptr", __PRETTY_FUNCTION__);
            return;
        }

        spdlog::info("{}, tag={:x}, colorL={:x}, colorH={:x}, x={}, y={}, scale={}, rotate={}",
            __PRETTY_FUNCTION__,
            info->emblem_texture_tag()->data()[0],
            info->emblem_color_l()->data()[0],
            info->emblem_color_h()->data()[0],
            info->emblem_x()->data()[0],
            info->emblem_y()->data()[0],
            info->emblem_scale()->data()[0],
            info->emblem_rotate()->data()[0]);

        EmblemInfo emblem_info;
        memcpy(emblem_info.emblemTextureTag, info->emblem_texture_tag()->data(), sizeof(uint32_t) * 4);
        memcpy(emblem_info.emblemColorL, info->emblem_color_l()->data(), sizeof(uint32_t) * 4);
        memcpy(emblem_info.emblemColorH, info->emblem_color_h()->data(), sizeof(uint32_t) * 4);
        memcpy(emblem_info.emblemX, info->emblem_x()->data(), sizeof(uint8_t) * 4);
        memcpy(emblem_info.emblemY, info->emblem_y()->data(), sizeof(uint8_t) * 4);
        memcpy(emblem_info.emblemScale, info->emblem_scale()->data(), sizeof(uint8_t) * 4);
        memcpy(emblem_info.emblemRotate, info->emblem_rotate()->data(), sizeof(uint8_t) * 4);

        g_hook->dynamiteCore.CreateEmblem(emblem_info);
    }

    void DynamiteSyncImpl::SyncVar(const std::string &catName, const std::string &varName) {
        if (!g_hook->cfg.Host || !CoopReady() || catName != "svars" ||
            std::ranges::find(enemyVars, varName) == enemyVars.end()) return;
        const auto values = g_hook->dynamiteCore.GetSVar(catName, varName);
        const auto publish = [&](const auto &items, DynamiteMessage::ArrayValues type, auto createValues) {
            for (size_t start = 0; start < items.size(); start += 128) {
                const auto count = std::min(size_t{128}, items.size() - start);
                using Value = typename std::decay_t<decltype(items)>::value_type;
                const std::vector<Value> chunk(items.begin() + start, items.begin() + start + count);
                flatbuffers::FlatBufferBuilder builder(768);
                const auto category = builder.CreateString(catName);
                const auto name = builder.CreateString(varName);
                const auto data = createValues(builder, chunk);
                const auto variable = DynamiteMessage::CreateSyncVar(builder, category, name, items.size(), start, type, data);
                const auto packet = packetNumber.fetch_add(1) + 1;
                builder.Finish(WrapMessage(builder, packet, DynamiteMessage::Message_SyncVar, variable.Union()));
                if (!SendRaw(&builder)) return;
            }
        };
        switch (values.type) {
#define SEND_VALUES(Type, Field, Name) case Type: publish(values.Field, DynamiteMessage::ArrayValues_##Name, [](auto &b, const auto &v) { return DynamiteMessage::Create##Name(b, b.CreateVector(v)).Union(); }); break;
            SEND_VALUES(TYPE_BOOL, bools, Bool)
            SEND_VALUES(TYPE_INT32, int32s, Int32)
            SEND_VALUES(TYPE_UINT32, uint32s, Uint32)
            SEND_VALUES(TYPE_FLOAT, floats, Float)
            SEND_VALUES(TYPE_INT8, int8s, Int8)
            SEND_VALUES(TYPE_UINT8, uint8s, Uint8)
            SEND_VALUES(TYPE_INT16, int16s, Int16)
            SEND_VALUES(TYPE_UINT16, uint16s, Uint16)
#undef SEND_VALUES
        default: break;
        }
    }
    void DynamiteSyncImpl::HandleSyncVar(const DynamiteMessage::MessageWrapper *w) {
        const auto m = w->msg_as_SyncVar();
        if (!m || !m->name() || !m->category() || !ValidMessage(w) || g_hook->cfg.Host) return;
        ScriptVariableHandle local{};
        // The native wrapper dispatches on the category hash; its first argument is unused.
        TppGmImplScriptSystemImplGetScriptDeclVarHandle(nullptr, &local,
            static_cast<uint32_t>(FoxStrHash32(m->category()->c_str(), m->category()->size())),
            static_cast<uint32_t>(FoxStrHash32(m->name()->c_str(), m->name()->size())));
        if (local.base == 0 || local.count != m->array_size() ||
            static_cast<unsigned>(m->array_values_type()) != (local.flags & 7u) + 1u) return;
        if (const auto floats = m->array_values_as_Float()) {
            for (const auto value : *floats->value()) if (!std::isfinite(value)) return;
        }
        spdlog::info("{}: {}.{}, size {}, start {}, type {}",
            __PRETTY_FUNCTION__,
            m->category()->c_str(),
            m->name()->c_str(),
            m->array_size(),
            m->array_start(),
            static_cast<short>(m->array_values_type()));

        void *vars = TppGmGetSVars();

        if (vars == nullptr) {
            spdlog::error("{}, bad category {}", __PRETTY_FUNCTION__, m->category()->c_str());
            return;
        }

        auto name = FoxStrHash32(m->name()->c_str(), m->name()->size()) & 0xffffffff;
        auto varIndex = TppGmImplScriptDeclVarsImplGetVarIndexWithName(vars, name);
        spdlog::info("{}, {}.{} index {}", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str(), varIndex);
        if (varIndex == 0xffffffff) {
            spdlog::error("{}, cannot get var index for {}.{} ({:x})", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str(), name);
            return;
        }

        bool res = false;
        switch (m->array_values_type()) {
        case DynamiteMessage::ArrayValues_Bool:
            res = SyncBoolVar(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Int32:
            res = SyncInt32Var(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Uint32:
            res = SyncUint32Var(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Float:
            res = SyncFloatVar(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Int8:
            res = SyncInt8Var(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Uint8:
            res = SyncUint8Var(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Int16:
            res = SyncInt16Var(m, vars, varIndex);
            break;
        case DynamiteMessage::ArrayValues_Uint16:
            res = SyncUint16Var(m, vars, varIndex);
            break;
        default:
            spdlog::error("{}, unknown array values type", __PRETTY_FUNCTION__);
            break;
        }

        if (res) {
            auto &received = syncReceived[m->name()->str()];
            if (received.size() != local.count) received.assign(local.count, false);
            uint32_t count = 0;
            switch (m->array_values_type()) {
#define CHUNK_COUNT(Name) case DynamiteMessage::ArrayValues_##Name: count = m->array_values_as_##Name()->value()->size(); break;
                CHUNK_COUNT(Bool) CHUNK_COUNT(Int32) CHUNK_COUNT(Uint32) CHUNK_COUNT(Float)
                CHUNK_COUNT(Int8) CHUNK_COUNT(Uint8) CHUNK_COUNT(Int16) CHUNK_COUNT(Uint16)
#undef CHUNK_COUNT
            default: break;
            }
            for (uint32_t i = 0; i < count; ++i) received[m->array_start() + i] = true;
            syncStatus[m->name()->str()] = std::ranges::all_of(received, [](bool value) { return value; });
        }
    }

    bool DynamiteSyncImpl::SyncBoolVar(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto asBool = m->array_values_as_Bool();
        if (asBool == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = asBool->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncInt32Var(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto asInt32 = m->array_values_as_Int32();
        if (asInt32 == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = asInt32->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncUint32Var(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto as32 = m->array_values_as_Uint32();
        if (as32 == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = as32->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        // just for logging
        g_hook->dynamiteCore.GetSVar(m->category()->c_str(), m->name()->c_str());

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            spdlog::info("{}, {}: {}", __PRETTY_FUNCTION__, i, val);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        // just for logging
        g_hook->dynamiteCore.GetSVar(m->category()->c_str(), m->name()->c_str());

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncInt16Var(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto as16 = m->array_values_as_Int16();
        if (as16 == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = as16->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncUint16Var(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto as16 = m->array_values_as_Uint16();
        if (as16 == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = as16->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncInt8Var(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto as8 = m->array_values_as_Int8();
        if (as8 == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = as8->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncUint8Var(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto as8 = m->array_values_as_Uint8();
        if (as8 == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = as8->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, val);
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    bool DynamiteSyncImpl::SyncFloatVar(const DynamiteMessage::SyncVar *m, void *vars, const uint32_t varIndex) {
        const auto asFloat = m->array_values_as_Float();
        if (asFloat == nullptr) {
            spdlog::error("{}, array values is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto values = asFloat->value();
        if (values == nullptr) {
            spdlog::error("{}, values is null", __PRETTY_FUNCTION__);
            return false;
        }

        if (values->size() < 1) {
            spdlog::error("{}, {}.{} size is 0!", __PRETTY_FUNCTION__, m->category()->c_str(), m->name()->c_str());
            return false;
        }

        spdlog::info("{}, setting {}.{}", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        for (size_t i = 0; i < values->size(); i++) {
            const auto val = values->Get(i);
            ScriptDeclVarsImplSetVarValue(vars, varIndex, m->array_start() + i, 0, std::bit_cast<uint32_t>(val));
        }

        spdlog::info("{}, {}.{}: done", __PRETTY_FUNCTION__, m->name()->c_str(), m->category()->c_str());
        return true;
    }

    void DynamiteSyncImpl::SyncEnemyVars() {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        for (const auto &v : enemyVars) {
            RequestVar("svars", v);
        }
    }

    void DynamiteSyncImpl::SendBossquietActionCommand(const uint32_t p1, BossQuietActionCommand *command) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(256);
        const std::vector<int8_t> vec(std::begin(command->data), std::end(command->data));
        const auto v = builder.CreateVector(vec);
        const auto sv = DynamiteMessage::CreateSendBossQuietActionCommand(builder, p1, v);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SendBossQuietActionCommand, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, res {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSendBossQuietActionCommand(const DynamiteMessage::MessageWrapper *w) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        if (hookState.bossQuietImplActionController == nullptr) {
            spdlog::error("{}, bossQuietImplActionController is nullptr", __PRETTY_FUNCTION__);
            return;
        }

        const auto m = w->msg_as_SendBossQuietActionCommand();
        if (!m || !m->action_command() || m->action_command()->size() != sizeof(BossQuietActionCommand)) return;
        const auto command = m->action_command();
        BossQuietActionCommand cmd{};
        memcpy(&cmd, command->data(), sizeof(cmd));
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}, {}", __PRETTY_FUNCTION__, bytes_to_hex(&cmd, sizeof(cmd)));
        }

        TppGmBossquietImplActionControllerImplSetCommand((char *)hookState.bossQuietImplActionController + 0x20, m->param_1(), &cmd);
    }

    void DynamiteSyncImpl::SendBossquietExtraActionCommand(const uint32_t p1, BossQuietActionCommand *command) {
        // always log, never seen this one
        spdlog::info("{}", __PRETTY_FUNCTION__);

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(256);
        const std::vector<uint8_t> vec(std::begin(command->data), std::end(command->data));
        const auto v = builder.CreateVector(vec);
        const auto sv = DynamiteMessage::CreateSendBossQuietExtraActionCommand(builder, p1, v);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SendBossQuietExtraActionCommand, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, res {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSendBossQuietExtraActionCommand(const DynamiteMessage::MessageWrapper *w) {
        // always log, never seen this one
        spdlog::info("{}", __PRETTY_FUNCTION__);

        if (hookState.bossQuietImplActionController == nullptr) {
            spdlog::error("{}, bossQuietImplActionController is nullptr", __PRETTY_FUNCTION__);
            return;
        }

        const auto m = w->msg_as_SendBossQuietExtraActionCommand();
        if (!m || !m->action_command() || m->action_command()->size() != sizeof(BossQuietActionCommand)) return;
        const auto command = m->action_command();
        BossQuietActionCommand cmd{};
        memcpy(&cmd, command->data(), sizeof(cmd));
        TppGmBossquietImplActionControllerImplSetExtraActionCommand((char *)hookState.bossQuietImplActionController + 0x20, m->param_1(), &cmd);
    }

    void DynamiteSyncImpl::SendBossquietSetNextActionTask(
        const uint32_t param_1, BossQuietActionTask *actionTask, BossQuietNextActionTaskActionCondition actionCondition) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}, task=\n{}\ndump={}", __PRETTY_FUNCTION__, actionTask->ToString(), bytes_to_hex(actionTask, sizeof(BossQuietActionTask)));
        }

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128 * 7);

        const auto byte_span = std::as_bytes(std::span{actionTask, 1});
        const std::vector<unsigned char> taskVec(
            reinterpret_cast<const unsigned char *>(byte_span.data()), reinterpret_cast<const unsigned char *>(byte_span.data()) + byte_span.size());
        const auto tv = builder.CreateVector(taskVec);

        auto at = DynamiteMessage::BossQuietNextActionTaskActionCondition_out_of_nav;
        switch (actionCondition) {
        case BossQuietNextActionTaskActionCondition::out_of_nav:
            at = DynamiteMessage::BossQuietNextActionTaskActionCondition_out_of_nav;
            break;
        case BossQuietNextActionTaskActionCondition::in_nav_with_state:
            at = DynamiteMessage::BossQuietNextActionTaskActionCondition_in_nav_with_state;
            break;
        case BossQuietNextActionTaskActionCondition::in_nav_without_state:
            at = DynamiteMessage::BossQuietNextActionTaskActionCondition_in_nav_without_state;
            break;
        default:
            spdlog::error("{}, unexpected action task value {}", __PRETTY_FUNCTION__, (int)actionCondition);
            return;
        }

        const auto sv = DynamiteMessage::CreateSendBossQuietSetNextActionTask(builder, param_1, tv, at);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SendBossQuietSetNextActionTask, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, res {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSendBossquietSetNextActionTask(const DynamiteMessage::MessageWrapper *w) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        if (hookState.bossQuietImplActionController == nullptr) {
            spdlog::error("{}, bossQuietImplActionController is nullptr", __PRETTY_FUNCTION__);
            return;
        }

        const auto m = w->msg_as_SendBossQuietSetNextActionTask();
        if (!m || !m->action_task() || m->action_task()->size() != sizeof(BossQuietActionTask)) return;

        BossQuietActionTask action_task{};
        memcpy(&action_task, m->action_task()->data(), sizeof(BossQuietActionTask));

        auto cond = BossQuietNextActionTaskActionCondition::out_of_nav;
        switch (m->action_type()) {
        case DynamiteMessage::BossQuietNextActionTaskActionCondition_out_of_nav:
            cond = BossQuietNextActionTaskActionCondition::out_of_nav;
            break;
        case DynamiteMessage::BossQuietNextActionTaskActionCondition_in_nav_with_state:
            cond = BossQuietNextActionTaskActionCondition::in_nav_with_state;
            break;
        case DynamiteMessage::BossQuietNextActionTaskActionCondition_in_nav_without_state:
            cond = BossQuietNextActionTaskActionCondition::in_nav_without_state;
            break;
        default:
            spdlog::error("{}, unexpected action task value {}", __PRETTY_FUNCTION__, (int)m->action_type());
            return;
        }

        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}, {}", __PRETTY_FUNCTION__, bytes_to_hex(&action_task, sizeof(action_task)));
        }

        g_hook->dynamiteCore.BossQuietSetNextActionTask(m->param_1(), &action_task, cond);
    }

    void DynamiteSyncImpl::SendBossquietSetActionTask(const uint32_t param_1, BossQuietActionTask *task) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128 * 7);

        const auto byte_span = std::as_bytes(std::span{task, 1});
        const std::vector<unsigned char> taskVec(
            reinterpret_cast<const unsigned char *>(byte_span.data()), reinterpret_cast<const unsigned char *>(byte_span.data()) + byte_span.size());
        const auto tv = builder.CreateVector(taskVec);

        const auto sv = DynamiteMessage::CreateSendBossQuietSetActionTask(builder, param_1, tv);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SendBossQuietSetActionTask, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, res {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSendBossquietSetActionTask(const DynamiteMessage::MessageWrapper *w) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        if (hookState.bossQuietImplActionController == nullptr) {
            spdlog::error("{}, bossQuietImplActionController is nullptr", __PRETTY_FUNCTION__);
            return;
        }

        const auto m = w->msg_as_SendBossQuietSetActionTask();
        if (!m || !m->task() || m->task()->size() != sizeof(BossQuietActionTask)) return;

        const auto workPtr = (void *)((char *)hookState.bossQuietImplActionController + 0x20 + 0x48);
        BossQuietActionTask task{};
        memcpy(&task, m->task()->data(), sizeof(task));

        // no controller offset!
        TppGmBossquietImplActionControllerImplSetActionTask(
            hookState.bossQuietImplActionController, m->param_1(), (BossQuietImplActionControllerImplWork *)workPtr, &task);
    }

    void DynamiteSyncImpl::SendBossQuietDamage(BossQuietDamage *damage) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128);

        const auto sv = DynamiteMessage::CreateSendBossQuietDamage(
            builder, damage->entityIndex, damage->currentLife, damage->lifeDamage, damage->currentStamina, damage->staminaDamage);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_SendBossQuietDamage, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}, res {}", __PRETTY_FUNCTION__, res);
    }

    void DynamiteSyncImpl::HandleSendBossQuietDamage(const DynamiteMessage::MessageWrapper *w) {
        if (g_hook->cfg.debug.bossQuiet) {
            spdlog::info("{}", __PRETTY_FUNCTION__);
        }

        const auto m = w->msg_as_SendBossQuietDamage();

        hookState.incomingBossQuietDamage.entityIndex = m->entity_index();
        hookState.incomingBossQuietDamage.lifeDamage = m->life_damage();
        hookState.incomingBossQuietDamage.staminaDamage = m->stamina_damage();
        hookState.incomingBossQuietDamage.currentLife = m->current_life();
        hookState.incomingBossQuietDamage.currentStamina = m->current_stamina();
    }

    // sends data over GameSocket, unused and deprecated
    bool DynamiteSyncImpl::Send(const flatbuffers::FlatBufferBuilder *builder) const {
        if (this->gameSocket == nullptr) {
            spdlog::info("{}, socket is null", __PRETTY_FUNCTION__);
            return false;
        }

        uint8_t *buf = builder->GetBufferPointer();
        int size = builder->GetSize();
        if (size == 0 || size > 1000) {
            spdlog::info("{}, invalid size {}", __PRETTY_FUNCTION__, size);
            return false;
        }

        if (buf == nullptr) {
            spdlog::info("{}, buffer is null", __PRETTY_FUNCTION__);
            return false;
        }

        // spdlog::info("{}: {} {}", __PRETTY_FUNCTION__, size, bytes_to_hex(buf, size));
        // auto wrapper = DynamiteMessage::GetMessageWrapper(buf);
        // switch (wrapper->msg_type()) {
        // case DynamiteMessage::Message_SendBossQuietSetActionTask: {
        //     spdlog::info("{}, sync var", __PRETTY_FUNCTION__);
        //     HandleSendBossquietSetActionTask(wrapper);
        //     break;
        // }
        // default:
        //     break;
        // }

        const auto session = FoxNtSessionGetMainSession();
        if (session == nullptr) {
            spdlog::error("{} Main session is null", __PRETTY_FUNCTION__);
            return false;
        }

        const auto session2impl = (void *)((char *)session + 0x20);
        if (session2impl == nullptr) {
            spdlog::error("{} Session2Impl is null", __PRETTY_FUNCTION__);
            return false;
        }

        auto index = 0;
        if (g_hook->cfg.Host) {
            index = 1;
        }

        // TODO replace with something like transceiver manager does? or player sync?
        FoxNtImplGameSocketImplRequestToSendToMemberHook(gameSocket, index, 0, buf, size);
        return true;
    }

    void DynamiteSyncImpl::AddFixedUserMarker(const Vector3 *pos) {
        spdlog::info("{}, {} {} {}", __PRETTY_FUNCTION__, pos->x, pos->y, pos->z);
        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(256);

        DynamiteMessage::Vec3 p(pos->x, pos->y, pos->z);
        auto sv = DynamiteMessage::CreateAddFixedUserMarker(builder, &p);
        auto message = WrapMessage(builder, packet, DynamiteMessage::Message_AddFixedUserMarker, sv.Union());
        builder.Finish(message);
        auto res = SendRaw(&builder);
        spdlog::info("{}: {}", __PRETTY_FUNCTION__, res);
    }

    // periodic sync is sent only while the partner is in the session: the socket outlives a leaving partner,
    // queued packets then flood the next join and its handshake times out
    bool DynamiteSyncImpl::PartnerInSession() const { return steamUDPSocket != nullptr && g_hook->dynamiteCore.GetMemberCount() >= 2; }

    void DynamiteSyncImpl::SendCarryDrop(const uint32_t objectID, const Vector3 *pos, const float rotY) {
        if (!PartnerInSession()) {
            return;
        }

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128);
        const auto p = DynamiteMessage::Vec3(pos->x, pos->y, pos->z);
        const auto sv = DynamiteMessage::CreateCarryDrop(builder, objectID, &p, rotY);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_CarryDrop, sv.Union());
        builder.Finish(message);
        const auto res = SendRaw(&builder);
        spdlog::info("{}: object {}, {:.1f} {:.1f} {:.1f}, sent {}", __PRETTY_FUNCTION__, objectID, pos->x, pos->y, pos->z, res);
    }

    void DynamiteSyncImpl::HandleCarryDrop(const DynamiteMessage::MessageWrapper *w) {
        const auto m = w->msg_as_CarryDrop();
        if (m == nullptr || m->pos() == nullptr) {
            spdlog::error("{}, malformed message", __PRETTY_FUNCTION__);
            return;
        }

        const auto pos = m->pos();
        std::lock_guard lock(carryDropsMutex);
        // lua drains this every frame; a full queue means lua is not running (loading), old drops are stale anyway
        if (carryDrops.size() >= 32) {
            carryDrops.pop_front();
        }

        carryDrops.push_back({m->object_id(), {.x = pos->x(), .y = pos->y(), .z = pos->z()}, m->rot_y()});
    }

    std::optional<CarryDropInfo> DynamiteSyncImpl::PopCarryDrop() {
        std::lock_guard lock(carryDropsMutex);
        if (carryDrops.empty()) {
            return std::nullopt;
        }

        auto res = carryDrops.front();
        carryDrops.pop_front();
        return res;
    }

    void DynamiteSyncImpl::SendVehicleState(const VehicleStateInfo &v) {
        using namespace std::chrono_literals;
        if (!CoopReady() || !VehicleID(v.objectID) || !FinitePosition(v.pos.x, v.pos.y, v.pos.z, v.rotY)) {
            return;
        }

        // moving vehicles 10 times per second, standing ones every 5 seconds
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(vehicleStatesMutex);
            const auto it = vehicleSent.find(v.objectID);
            if (it != vehicleSent.end()) {
                const auto elapsed = now - it->second.time;
                const auto &old = it->second.state;
                const auto dx = v.pos.x - old.pos.x, dy = v.pos.y - old.pos.y, dz = v.pos.z - old.pos.z;
                const auto changed = dx * dx + dy * dy + dz * dz > 0.1f * 0.1f || std::abs(v.rotY - old.rotY) > 0.01f || v.driven != old.driven || v.hasRot != old.hasRot;
                if (elapsed < 100ms || (!changed && elapsed < (v.driven ? 500ms : 5s))) {
                    return;
                }
            }

        }

        const auto packet = packetNumber.fetch_add(1) + 1;
        flatbuffers::FlatBufferBuilder builder(128);
        const auto p = DynamiteMessage::Vec3(v.pos.x, v.pos.y, v.pos.z);
        const auto sv = DynamiteMessage::CreateVehicleState(builder, v.objectID, &p, v.rotY, v.hasRot, v.driven);
        const auto message = WrapMessage(builder, packet, DynamiteMessage::Message_VehicleState, sv.Union());
        builder.Finish(message);
        if (SendRaw(&builder)) {
            std::lock_guard lock(vehicleStatesMutex);
            vehicleSent[v.objectID] = {now, v};
        }
    }

    void DynamiteSyncImpl::HandleVehicleState(const DynamiteMessage::MessageWrapper *w) {
        const auto m = w->msg_as_VehicleState();
        if (m == nullptr || m->pos() == nullptr) {
            spdlog::error("{}, malformed message", __PRETTY_FUNCTION__);
            return;
        }

        const auto pos = m->pos();
        std::lock_guard lock(vehicleStatesMutex);
        vehicleStates[m->object_id()] = {m->object_id(), {.x = pos->x(), .y = pos->y(), .z = pos->z()}, m->rot_y(), m->has_rot(), m->driven()};
    }

    std::optional<VehicleStateInfo> DynamiteSyncImpl::PopVehicleState() {
        std::lock_guard lock(vehicleStatesMutex);
        if (vehicleStates.empty()) {
            return std::nullopt;
        }

        const auto it = vehicleStates.begin();
        auto res = it->second;
        vehicleStates.erase(it);
        return res;
    }

    bool DynamiteSyncImpl::SendRaw(const flatbuffers::FlatBufferBuilder *builder) const {
        if (builder == nullptr) return false;
        // SteamUdpSocketImpl::Send (0x141a5fb60) ignores argument four.
        // Native calls legitimately supply nullptr for SocketInfo; requiring it
        // prevents every custom packet, including the initial co-op handshake.
        if (steamUDPSocket == nullptr || steamUDPAddress == nullptr) {
            spdlog::debug("{}, transport endpoint unavailable (socket={}, address={})", __PRETTY_FUNCTION__, steamUDPSocket, steamUDPAddress);
            return false;
        }

        auto buf = builder->GetBufferPointer();
        auto size = builder->GetSize();
        if (size == 0 || size > MaxSyncPayload) {
            spdlog::info("{}, invalid size {}", __PRETTY_FUNCTION__, size);
            return false;
        }

        if (buf == nullptr) {
            spdlog::info("{}, buffer is null", __PRETTY_FUNCTION__);
            return false;
        }

        // auto wrapper = DynamiteMessage::GetMessageWrapper(buf);
        // switch (wrapper->msg_type()) {
        // case DynamiteMessage::Message_SendBossQuietSetActionTask: {
        //     spdlog::info("{}, test handling", __PRETTY_FUNCTION__);
        //     HandleSendBossquietSetActionTask(wrapper);
        //     break;
        // }
        // default:
        //     break;
        // }

        auto bfSize = size + sizeof(DYNAMITE_RAW_HEADER);
        const auto newBuf = malloc(bfSize);
        if (newBuf == nullptr) {
            spdlog::error("{}, malloc failed", __PRETTY_FUNCTION__);
            return false;
        }

        memset(newBuf, 0, bfSize);
        memcpy(newBuf, DYNAMITE_RAW_HEADER, sizeof(DYNAMITE_RAW_HEADER));
        memcpy((char *)newBuf + sizeof(DYNAMITE_RAW_HEADER), buf, size);

        if (g_hook->cfg.debug.dynamiteMsg) {
            auto dump = bytes_to_hex(newBuf, bfSize);
            spdlog::info("{}, size={}, bufSize={}, dump={}", __PRETTY_FUNCTION__, size, bfSize, dump);
        }

        // auto snf = *(SteamNetworkingFunc*)0x14db4f7c8;
        // auto networking = snf();
        const auto sent = FoxNioImplSteamUdpSocketImplSend(steamUDPSocket, newBuf, bfSize, steamUDPSocketInfo, steamUDPAddress);

        free(newBuf);

        return sent == static_cast<int32_t>(bfSize);
    }

    bool DynamiteSyncImpl::RecvRaw(void *buffer, int32_t size) {
        if (!buffer || size <= static_cast<int32_t>(sizeof(DYNAMITE_RAW_HEADER)) ||
            size > static_cast<int32_t>(MaxSyncPayload + sizeof(DYNAMITE_RAW_HEADER)) ||
            std::memcmp(buffer, DYNAMITE_RAW_HEADER, sizeof(DYNAMITE_RAW_HEADER)) != 0) return false;
        const auto begin = static_cast<const uint8_t *>(buffer) + sizeof(DYNAMITE_RAW_HEADER);
        const auto length = size - sizeof(DYNAMITE_RAW_HEADER);
        // Copy into owned, aligned storage before verification; never verify padding beyond the received bytes.
        std::vector<uint8_t> data(begin, begin + length);
        flatbuffers::Verifier verifier(data.data(), data.size());
        if (!DynamiteMessage::VerifyMessageWrapperBuffer(verifier) || !ValidMessage(DynamiteMessage::GetMessageWrapper(data.data()))) return false;
        std::lock_guard lock(incomingMutex);
        if (incoming.size() >= 128) incoming.pop_front();
        incoming.push_back(std::move(data));
        return true;
    }

    void DynamiteSyncImpl::ProcessIncoming() {
        std::lock_guard runtimeLock(runtimeMutex);
        if (!missionPlaying || !PartnerInSession()) return;
        std::deque<std::vector<uint8_t>> pending;
        { std::lock_guard lock(incomingMutex); pending.swap(incoming); }
        for (const auto &data : pending) {
        const auto wrapper = DynamiteMessage::GetMessageWrapper(data.data());
        if (wrapper->mission_code() != missionCode || wrapper->sender_epoch() == 0) continue;
        if (wrapper->msg_type() == DynamiteMessage::Message_CoopHello) {
            const auto hello = wrapper->msg_as_CoopHello();
            if (hello->host() == g_hook->cfg.Host) continue;
            // Epochs are monotonic on a peer. A delayed hello must not roll its binding back.
            if (peerEpoch != 0 && wrapper->sender_epoch() < peerEpoch) continue;
            pendingPeerEpoch = wrapper->sender_epoch();
            if (hello->echo_epoch() == localEpoch && peerEpoch != wrapper->sender_epoch()) {
                peerEpoch = wrapper->sender_epoch();
                replay = {};
                questSequence = 0;
                questState.reset();
                buddyScans.Reset();
                dogReplica.Reset();
                entitySequences.clear();
                spdlog::info("co-op protocol ready: mission {}, peer epoch {}", missionCode.load(), peerEpoch.load());
                SendEmblem();
            }
            continue;
        }
        if (wrapper->sender_epoch() != peerEpoch || wrapper->receiver_epoch() != localEpoch || !replay.Accept(wrapper->packet_num())) continue;
        const auto type = wrapper->msg_type();
        // Only the host publishes world/quest state. Clients may request world generation variables.
        if ((type == DynamiteMessage::Message_SyncVar || type == DynamiteMessage::Message_QuestState ||
            (type >= DynamiteMessage::Message_SendBossQuietActionCommand && type <= DynamiteMessage::Message_SendBossQuietSetActionTask)) && g_hook->cfg.Host) continue;
        if (type == DynamiteMessage::Message_RequestVar && !g_hook->cfg.Host) continue;
        if (type == DynamiteMessage::Message_CarryDrop || type == DynamiteMessage::Message_VehicleState) {
            const auto id = type == DynamiteMessage::Message_CarryDrop ? wrapper->msg_as_CarryDrop()->object_id() : wrapper->msg_as_VehicleState()->object_id();
            auto &last = entitySequences[id];
            if (!NewerSequence(wrapper->packet_num(), last)) continue;
            last = wrapper->packet_num();
        }
        switch (wrapper->msg_type()) {
        case DynamiteMessage::Message_DogReplicaPose: {
            const auto part=DecodeDogPose(wrapper->msg_as_DogReplicaPose());
            if(part && part->pose.owner!=(g_hook->cfg.Host?0:1)) dogReplica.Receive(*part,std::chrono::steady_clock::now());
            break;
        }
        case DynamiteMessage::Message_BuddyScans: {
            const auto ids=wrapper->msg_as_BuddyScans()->object_ids();
            buddyScans.Receive(wrapper->packet_num(),{ids->begin(),ids->end()},std::chrono::steady_clock::now());
            break;
        }
        case DynamiteMessage::Message_QuestState: {
            if (missionCode != 30010 && missionCode != 30020) break;
            if (!NewerSequence(wrapper->packet_num(), questSequence)) break;
            questSequence = wrapper->packet_num();
            const auto q = wrapper->msg_as_QuestState();
            questState = QuestStateInfo{q->name()->str(), q->step()->str(), q->active(), q->invoked(), q->face_id()};
            break;
        }
        case DynamiteMessage::Message_Ping:
            spdlog::info("{}, ping", __PRETTY_FUNCTION__);
            break;
        case DynamiteMessage::Message_AddFixedUserMarker:
            HandleAddFixedUserMarker(wrapper);
            break;
        case DynamiteMessage::Message_AddFollowUserMarker:
            HandleAddFollowUserMarker(wrapper);
            break;
        case DynamiteMessage::Message_RemoveUserMarker:
            HandleRemoveUserMarker(wrapper);
            break;
        case DynamiteMessage::Message_SetSightMarker:
            HandleSetSightMarker(wrapper);
            break;
        case DynamiteMessage::Message_SyncVar:
            HandleSyncVar(wrapper);
            break;
        case DynamiteMessage::Message_RequestVar:
            HandleRequestVar(wrapper);
            break;
        case DynamiteMessage::Message_SendEmblem:
            HandleSendEmblem(wrapper);
            break;
        case DynamiteMessage::Message_SendBossQuietActionCommand:
            HandleSendBossQuietActionCommand(wrapper);
            break;
        case DynamiteMessage::Message_SendBossQuietExtraActionCommand:
            HandleSendBossQuietExtraActionCommand(wrapper);
            break;
        case DynamiteMessage::Message_SendBossQuietSetNextActionTask:
            HandleSendBossquietSetNextActionTask(wrapper);
            break;
        case DynamiteMessage::Message_SendBossQuietSetActionTask:
            HandleSendBossquietSetActionTask(wrapper);
            break;
        case DynamiteMessage::Message_SendBossQuietDamage:
            HandleSendBossQuietDamage(wrapper);
            break;
        case DynamiteMessage::Message_CarryDrop:
            HandleCarryDrop(wrapper);
            break;
        case DynamiteMessage::Message_VehicleState:
            HandleVehicleState(wrapper);
            break;
        default:
            spdlog::error("{}, unknown message type {}", __PRETTY_FUNCTION__, static_cast<uint32_t>(wrapper->msg_type()));
            break;
        }

        }
    }

    flatbuffers::Offset<DynamiteMessage::MessageWrapper> DynamiteSyncImpl::WrapMessage(flatbuffers::FlatBufferBuilder &builder,
        uint32_t packet, DynamiteMessage::Message type, flatbuffers::Offset<void> message) const {
        return DynamiteMessage::CreateMessageWrapper(builder, packet, type, message, missionCode.load(), localEpoch.load(), peerEpoch.load());
    }

    void DynamiteSyncImpl::SetMissionContext(uint32_t mission, bool playing) {
        std::lock_guard lock(runtimeMutex);
        if(missionCode!=mission || localEpoch==0) { buddyScans.Reset(true); dogReplica.Reset(true); }
        // A disconnected session can rotate epochs without unloading the local Dog.
        // Actual same-mission reloads clear local detections through the Lua world lifecycle.
        if (!playing || missionCode != mission || localEpoch == 0) Init();
        missionCode = mission;
        missionPlaying = playing;
    }

    bool DynamiteSyncImpl::CoopReady() const { return missionPlaying && peerEpoch != 0 && PartnerInSession(); }

    void DynamiteSyncImpl::SendCoopHello() {
        if (!missionPlaying || !PartnerInSession()) return;
        flatbuffers::FlatBufferBuilder builder(128);
        const auto hello = DynamiteMessage::CreateCoopHello(builder, 4, g_hook->cfg.Host, pendingPeerEpoch.load());
        const auto packet = packetNumber.fetch_add(1) + 1;
        builder.Finish(WrapMessage(builder, packet, DynamiteMessage::Message_CoopHello, hello.Union()));
        SendRaw(&builder);
    }

    void DynamiteSyncImpl::SendQuestState(const QuestStateInfo &state) {
        if (!g_hook->cfg.Host || !CoopReady() || (missionCode != 30010 && missionCode != 30020)) return;
        flatbuffers::FlatBufferBuilder builder(256);
        const auto name = builder.CreateString(state.name);
        const auto step = builder.CreateString(state.step);
        const auto q = DynamiteMessage::CreateQuestState(builder, name, step, state.active, state.invoked, state.faceID);
        const auto packet = packetNumber.fetch_add(1) + 1;
        builder.Finish(WrapMessage(builder, packet, DynamiteMessage::Message_QuestState, q.Union()));
        SendRaw(&builder);
    }

    std::optional<QuestStateInfo> DynamiteSyncImpl::PopQuestState() {
        std::lock_guard lock(runtimeMutex);
        auto q = std::move(questState);
        questState.reset();
        return q;
    }

    void DynamiteSyncImpl::SendBuddyScans() {
        if(!CoopReady()) return;
        const auto ids=buddyScans.Snapshot();
        flatbuffers::FlatBufferBuilder b(256);
        const auto m=DynamiteMessage::CreateBuddyScans(b,b.CreateVector(ids));
        b.Finish(WrapMessage(b,packetNumber.fetch_add(1)+1,DynamiteMessage::Message_BuddyScans,m.Union()));
        SendRaw(&b);
    }
    void DynamiteSyncImpl::UpdateBuddyScans(bool active) {
        const auto changes=buddyScans.Frame(active && CoopReady(),std::chrono::steady_clock::now());
        for(const auto &change:changes) BuddyScans::Apply(change.id,change.enabled);
        if(active) SendBuddyScans();
    }

    void DynamiteSyncImpl::SendDogReplica(bool selected,uint32_t generation) {
        if(!CoopReady()) return;
        const auto pose=dogReplica.Outgoing(selected,g_hook->cfg.Host?0:1,generation,std::chrono::steady_clock::now());
        const size_t count=pose.bones.size();
        for(size_t first=0;first<std::max<size_t>(count,1);first+=DogBonesPerChunk) {
            const size_t length=std::min<size_t>(DogBonesPerChunk,count-first);
            flatbuffers::FlatBufferBuilder b(1024);
            const auto world=b.CreateVector(pose.world.data(),pose.active?16:0);
            std::vector<float> values(length*16);
            if(length) std::memcpy(values.data(),pose.bones.data()+first,length*sizeof(DogMatrix));
            const auto matrices=b.CreateVector(values);
            const DynamiteMessage::Vec3 position(pose.position[0],pose.position[1],pose.position[2]);
            const DynamiteMessage::BuddyRotation rotation(pose.rotation[0],pose.rotation[1],pose.rotation[2],pose.rotation[3]);
            const auto m=DynamiteMessage::CreateDogReplicaPose(b,pose.generation,pose.frame,pose.owner,pose.active,pose.space,count,first,world,matrices,pose.active?&position:nullptr,pose.active?&rotation:nullptr);
            b.Finish(WrapMessage(b,packetNumber.fetch_add(1)+1,DynamiteMessage::Message_DogReplicaPose,m.Union()));
            if(b.GetSize()<=MaxSyncPayload) SendRaw(&b);
        }
    }

}
