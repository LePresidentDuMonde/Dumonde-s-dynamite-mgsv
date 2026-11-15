#pragma once
#include "DogReplicaState.h"
#include "DynamiteSyncSchema_generated.h"

namespace Dynamite {
    inline std::optional<DogPosePart> DecodeDogPose(const DynamiteMessage::DogReplicaPose *m) {
        if(!m || !m->world() || !m->matrices() || m->world()->size()!=(m->active()?16:0) ||
           m->matrices()->size()>16*DogBonesPerChunk || m->matrices()->size()%16) return {};
        DogPosePart p; auto &v=p.pose;
        v.generation=m->generation(); v.frame=m->frame(); v.owner=m->owner(); v.space=m->space(); v.active=m->active();
        p.boneCount=m->bone_count(); p.firstBone=m->first_bone();
        if(v.active) {
            if(!m->position() || !m->rotation()) return {};
            v.position={m->position()->x(),m->position()->y(),m->position()->z()};
            v.rotation={m->rotation()->x(),m->rotation()->y(),m->rotation()->z(),m->rotation()->w()};
        }
        if(v.active) std::memcpy(v.world.data(),m->world()->data(),sizeof(DogMatrix));
        v.bones.resize(m->matrices()->size()/16);
        if(!v.bones.empty()) std::memcpy(v.bones.data(),m->matrices()->data(),v.bones.size()*sizeof(DogMatrix));
        return ValidDogPart(p)?std::optional<DogPosePart>(std::move(p)):std::nullopt;
    }
}
