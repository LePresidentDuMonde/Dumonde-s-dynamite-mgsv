#pragma once

#include "DynamiteSyncSchema_generated.h"
#include "SyncPolicy.h"
#include "DogPoseWire.h"
#include "Tpp/BossQuietActionCommand.h"
#include "Tpp/BossQuietActionTask.h"
#include <cstring>

namespace Dynamite {
    inline bool ValidString(const flatbuffers::String *value, size_t maximum, bool empty = false) {
        return value && value->size() <= maximum && (empty || value->size() > 0) &&
               std::memchr(value->data(), 0, value->size()) == nullptr;
    }

    inline bool ValidPosition(const DynamiteMessage::Vec3 *pos, float angle = 0) {
        return pos && FinitePosition(pos->x(), pos->y(), pos->z(), angle);
    }

    inline bool ValidVariable(const flatbuffers::String *category, const flatbuffers::String *name) {
        // Replicate world generation only. Never accept arbitrary writes into saves/progression.
        if (!ValidString(category, 5) || !ValidString(name, 64) || category->str() != "svars") return false;
        const auto value = name->str();
        return value == "solFovaSeed" || value == "solFaceFova" || value == "solBodyFova" || value == "solRandomSeed";
    }

    inline bool ValidMessage(const DynamiteMessage::MessageWrapper *w) {
        using namespace DynamiteMessage;
        if (!w || !w->msg() || w->packet_num() == 0) return false;
        switch (w->msg_type()) {
        case Message_Ping: return w->msg_as_Ping() != nullptr;
        case Message_CoopHello: return w->msg_as_CoopHello() && w->msg_as_CoopHello()->protocol() == 4 && w->sender_epoch() != 0;
        case Message_DogReplicaPose: return DecodeDogPose(w->msg_as_DogReplicaPose()).has_value();
        // IDs retained in the schema so older unions cannot alias the new scan message.
        case Message_BuddyState: case Message_BuddyOrder: case Message_BuddyAck: case Message_BuddyMotion: return false;
        case Message_BuddyScans: {
            const auto m=w->msg_as_BuddyScans(); const auto ids=m?m->object_ids():nullptr;
            if(!ids || ids->size()>MaxBuddyScans) return false;
            uint32_t previous=0;
            for(auto id:*ids) { if(!BuddyScanID(id) || id<=previous) return false; previous=id; }
            return true;
        }
        case Message_QuestState: {
            const auto m = w->msg_as_QuestState();
            return m && m->face_id() <= 65536 && ValidString(m->name(), 96, true) && ValidString(m->step(), 96, true) &&
                   (m->name()->size() != 0 || (m->step()->size() == 0 && !m->active() && !m->invoked()));
        }
        case Message_CarryDrop: {
            const auto m = w->msg_as_CarryDrop();
            return m && CarryID(m->object_id()) && ValidPosition(m->pos(), m->rot_y());
        }
        case Message_VehicleState: {
            const auto m = w->msg_as_VehicleState();
            return m && VehicleID(m->object_id()) && ValidPosition(m->pos(), m->rot_y());
        }
        case Message_RequestVar: {
            const auto m = w->msg_as_RequestVar();
            return m && ValidVariable(m->category(), m->name());
        }
        case Message_SyncVar: {
            const auto m = w->msg_as_SyncVar();
            if (!m || !ValidVariable(m->category(), m->name()) || m->array_size() == 0 || m->array_size() > 65535) return false;
            uint32_t count = 0;
            switch (m->array_values_type()) {
#define CHECK_VALUES(Name) case ArrayValues_##Name: { const auto v = m->array_values_as_##Name(); if (!v || !v->value()) return false; count = v->value()->size(); break; }
                CHECK_VALUES(Bool) CHECK_VALUES(Int32) CHECK_VALUES(Uint32) CHECK_VALUES(Float)
                CHECK_VALUES(Int8) CHECK_VALUES(Uint8) CHECK_VALUES(Int16) CHECK_VALUES(Uint16)
#undef CHECK_VALUES
            default: return false;
            }
            return count > 0 && m->array_start() <= m->array_size() && count <= m->array_size() - m->array_start();
        }
        case Message_AddFixedUserMarker: return ValidPosition(w->msg_as_AddFixedUserMarker()->pos());
        case Message_AddFollowUserMarker: return ValidPosition(w->msg_as_AddFollowUserMarker()->pos()) && w->msg_as_AddFollowUserMarker()->object_id() < 0xffff;
        case Message_RemoveUserMarker: return w->msg_as_RemoveUserMarker()->marker_id() < 5;
        case Message_SetSightMarker: {
            const auto m=w->msg_as_SetSightMarker();
            return m->object_id()<0xffff && std::isfinite(m->duration()) && m->duration()>=0 && m->duration()<=10000000;
        }
        case Message_SendEmblem: return w->msg_as_SendEmblem()->emblem_info() != nullptr;
        case Message_SendBossQuietActionCommand: {
            const auto v = w->msg_as_SendBossQuietActionCommand()->action_command();
            return v && v->size() == sizeof(BossQuietActionCommand);
        }
        case Message_SendBossQuietExtraActionCommand: {
            const auto v = w->msg_as_SendBossQuietExtraActionCommand()->action_command();
            return v && v->size() == sizeof(BossQuietActionCommand);
        }
        case Message_SendBossQuietSetNextActionTask: {
            const auto m = w->msg_as_SendBossQuietSetNextActionTask();
            return m->action_task() && m->action_task()->size() == sizeof(BossQuietActionTask) && m->action_type() >= 0 && m->action_type() <= 2;
        }
        case Message_SendBossQuietSetActionTask: {
            const auto v = w->msg_as_SendBossQuietSetActionTask()->task();
            return v && v->size() == sizeof(BossQuietActionTask);
        }
        case Message_SendBossQuietDamage: return w->msg_as_SendBossQuietDamage()->entity_index() == 0;
        default: return false;
        }
    }
}
