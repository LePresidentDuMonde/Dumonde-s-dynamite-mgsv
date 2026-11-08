#ifndef HOOK_PARTNERHORSE_H
#define HOOK_PARTNERHORSE_H

#include <cstdint>
#include <optional>

#include "Tpp/TppTypes.h"

// Partner's D-Horse: the game loads one buddy horse, this adds a second instance (horse 1) and drives it from partner's horse state.
// Also lets horses accept calls in online missions, so the whistle works in co-op.
namespace Dynamite::PartnerHorse {
    void Install();

    // position of the local buddy horse (horse 0), once the horse type was used this mission
    std::optional<Vector3> LocalHorse();

    // partner's horse is at pos, ridden or not; partner is player index partnerIndex
    void Follow(const Vector3 &pos, bool ridden, uint16_t partnerIndex);

    // partner has no horse: hide horse 1 and stop its AI
    void Hide();
} // namespace Dynamite::PartnerHorse

#endif // HOOK_PARTNERHORSE_H
