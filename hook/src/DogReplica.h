#pragma once
#include <cstdint>

namespace Dynamite::DogReplica {
    bool Install();
    bool Available();
    void Created();
    void BeforePre(void *root);
    void AfterPre(void *root);
    void Update(bool active,bool selected);
    void ResetWorld();
    uint32_t Generation();
}
