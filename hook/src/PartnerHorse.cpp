#include "PartnerHorse.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <windows.h>

#include "MinHook.h"
#include "spdlog/spdlog.h"

// Layout found by reverse engineering mgsvtpp.exe 1.0.15.4, "self" is the TppHorse2 type object the handlers receive:
//   +0x38  AI: runs agents 0..[ai+0x58], state bytes [ai+0x80][i] (bits 0-1 current, 2-3 requested, 1 = active)
//   +0x68  per horse table, 0x2f0 bytes each, count at table+0x10
//   +0xc4  flags, bit 2 set in online missions makes horses refuse calls
//   +0xf0  horses per group, +0x110 groups; group+0x10 actor; actor entry [actor+0x10] + (i - [actor+0xc]) * 0xc0
//   entry +0x60 / +0x68 components holding the horse position
namespace Dynamite::PartnerHorse {
    namespace {
        constexpr uint64_t imageBase = 0x140000000;
        constexpr uint64_t createTypeAddress = 0x1409e5470;       // generic game object type creation
        constexpr uint64_t commandHandlerAddress = 0x140e4d250;   // TppHorse2 lua commands
        constexpr uint64_t messageReceiverAddress = 0x140e4d960;  // TppHorse2 messages (whistle)
        constexpr uint64_t setAgentStatesAddress = 0x140c0f520;   // AI requested states, one per agent
        constexpr uint64_t setAgentStateAddress = 0x140c0f570;    // AI requested state for the first agents
        constexpr uint16_t horseTypeIndex = 13;                   // TppHorse2, game object id 6656 >> 9
        constexpr uint32_t horses = 2;
        constexpr uint64_t whistleMessage = 0x7c8cc24e84c3;       // +8: index of the whistling player
        constexpr float warpDistance = 25;
        constexpr float callDistance = 3;
        constexpr float idleDistance = 6;

        using CreateType = int64_t (*)(void *type, uint32_t *desc);
        using CommandHandler = int64_t (*)(void *self, void *lua, uint32_t index, uint32_t command);
        using MessageReceiver = void *(*)(void *self, void *result, uint32_t index, const uint64_t *message);
        using SetAgentStates = void (*)(uint8_t *ai, const int32_t *states, uint32_t count);
        using SetAgentState = void (*)(uint8_t *ai, uint32_t state, uint32_t count);
        CreateType originalCreateType = nullptr;
        CommandHandler originalCommand = nullptr;
        MessageReceiver originalMessage = nullptr;
        SetAgentStates originalSetStates = nullptr;
        SetAgentState originalSetState = nullptr;

        std::atomic<uint8_t *> horseType = nullptr; // known after the first command or message of the mission
        std::atomic<bool> active = false;           // partner has a horse, horse 1 is driven
        std::chrono::steady_clock::time_point lastCall;

        uint32_t TableCount(const uint8_t *self, int offset) {
            const auto table = *reinterpret_cast<uint8_t *const *>(self + offset);
            return table == nullptr ? 0 : *reinterpret_cast<const uint32_t *>(table + 0x10);
        }

        bool Readable(const void *p, size_t size) {
            MEMORY_BASIC_INFORMATION mbi{};
            if (p == nullptr || VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
                return false;
            }
            return static_cast<const uint8_t *>(p) + size <= static_cast<const uint8_t *>(mbi.BaseAddress) + mbi.RegionSize;
        }

        uint8_t *Component(const uint8_t *s, uint32_t i, int offset) {
            const auto perGroup = *reinterpret_cast<const uint32_t *>(s + 0xf0);
            const auto groups = *reinterpret_cast<uint8_t *const *const *>(s + 0x110);
            if (perGroup == 0 || i >= TableCount(s, 0x68) || !Readable(groups + i / perGroup, 8)) {
                return nullptr;
            }
            const auto group = groups[i / perGroup];
            if (!Readable(group, 0x18)) {
                return nullptr;
            }
            const auto actor = *reinterpret_cast<uint8_t *const *>(group + 0x10);
            if (!Readable(actor, 0x18)) {
                return nullptr;
            }
            const auto first = *reinterpret_cast<const uint32_t *>(actor + 0xc);
            const auto entry = *reinterpret_cast<uint8_t *const *>(actor + 0x10) + (i - first) * 0xc0;
            if (i < first || !Readable(entry, 0xc0)) {
                return nullptr;
            }
            const auto component = *reinterpret_cast<uint8_t *const *>(entry + offset);
            return Readable(component, 0x200) ? component : nullptr;
        }

        // the base field is the horse position, the others are kept at their offset from it
        struct PositionField {
            int component;
            int offset;
        };
        constexpr PositionField positionFields[] = {{0x60, 0x20}, {0x60, 0x30}, {0x60, 0x120}, {0x68, 0x70}, {0x68, 0xd0}};

        float *Position(const uint8_t *s, uint32_t i, const PositionField &f) {
            const auto c = Component(s, i, f.component);
            return c == nullptr ? nullptr : reinterpret_cast<float *>(c + f.offset);
        }

        void Place(const uint8_t *s, const Vector3 &pos) {
            const auto base = Position(s, 0, positionFields[0]);
            for (const auto &f : positionFields) {
                const auto p0 = Position(s, 0, f);
                const auto p1 = Position(s, 1, f);
                if (base == nullptr || p0 == nullptr || p1 == nullptr) {
                    return;
                }
                p1[0] = pos.x + p0[0] - base[0];
                p1[1] = pos.y + p0[1] - base[1];
                p1[2] = pos.z + p0[2] - base[2];
            }
        }

        // online missions set flag bit 2 at type creation (game state query 0x14), horses then refuse calls; restore offline state
        void AllowCalls(void *self) {
            auto &flags = *reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(self) + 0xc4);
            if (flags & 2) {
                flags = (flags & ~2u) | 1u;
            }
        }

        void Seen(void *self) {
            horseType = static_cast<uint8_t *>(self);
            AllowCalls(self);
        }

        int64_t CreateTypeHook(void *type, uint32_t *desc) {
            if (*reinterpret_cast<const uint16_t *>(static_cast<uint8_t *>(type) + 0x1c) == horseTypeIndex) {
                horseType = nullptr; // the old one is freed
                active = false;
                desc[1] = std::max(desc[1], horses); // total count
                desc[2] = std::max(desc[2], horses); // realized count
            }
            return originalCreateType(type, desc);
        }

        int64_t CommandHook(void *self, void *lua, uint32_t index, uint32_t command) {
            Seen(self);
            if (index >= TableCount(static_cast<uint8_t *>(self), 0x68)) {
                return 0; // the handler would read a null entry
            }
            return originalCommand(self, lua, index, command);
        }

        void *MessageHook(void *self, void *result, uint32_t index, const uint64_t *message) {
            Seen(self);
            return originalMessage(self, result, index, message);
        }

        // the game requests AI for the buddy (horse 0) only
        void RequestHorse1(uint8_t *ai) {
            const auto s = horseType.load();
            if (!active || s == nullptr || ai != *reinterpret_cast<uint8_t *const *>(s + 0x38) || *reinterpret_cast<const uint32_t *>(ai + 0x58) < horses) {
                return;
            }
            auto &state = (*reinterpret_cast<uint8_t *const *>(ai + 0x80))[1];
            state = (state & ~0x0c) | 0x04;
        }

        void SetStatesHook(uint8_t *ai, const int32_t *states, uint32_t count) {
            originalSetStates(ai, states, count);
            RequestHorse1(ai);
        }

        void SetStateHook(uint8_t *ai, uint32_t state, uint32_t count) {
            originalSetState(ai, state, count);
            RequestHorse1(ai);
        }

        template <typename T> bool Hook(uint64_t address, T *detour, T **original, uint64_t exe) {
            const auto target = reinterpret_cast<LPVOID>(address - imageBase + exe);
            return MH_CreateHook(target, reinterpret_cast<LPVOID>(detour), reinterpret_cast<LPVOID *>(original)) == MH_OK && MH_EnableHook(target) == MH_OK;
        }

        float DistanceSq(const float *a, const Vector3 &b) {
            const auto dx = a[0] - b.x, dy = a[1] - b.y, dz = a[2] - b.z;
            return dx * dx + dy * dy + dz * dz;
        }
    } // namespace

    void Install() {
        const auto exe = reinterpret_cast<uint64_t>(GetModuleHandle(nullptr));
        if (!Hook(createTypeAddress, CreateTypeHook, &originalCreateType, exe) || !Hook(commandHandlerAddress, CommandHook, &originalCommand, exe) ||
            !Hook(messageReceiverAddress, MessageHook, &originalMessage, exe) || !Hook(setAgentStatesAddress, SetStatesHook, &originalSetStates, exe) ||
            !Hook(setAgentStateAddress, SetStateHook, &originalSetState, exe)) {
            spdlog::error("partner horse: hook failed");
        }
    }

    std::optional<Vector3> LocalHorse() {
        const auto s = horseType.load();
        const auto p = s == nullptr ? nullptr : Position(s, 0, positionFields[0]);
        if (p == nullptr) {
            return std::nullopt;
        }
        return Vector3{.x = p[0], .y = p[1], .z = p[2]};
    }

    void Follow(const Vector3 &pos, bool ridden, uint16_t partnerIndex) {
        const auto s = horseType.load();
        const auto current = s == nullptr ? nullptr : Position(s, 1, positionFields[0]);
        if (current == nullptr) {
            return;
        }

        if (!active) {
            spdlog::info("partner horse: following partner's horse");
            active = true;
        }

        const auto d = DistanceSq(current, pos);
        if (d > warpDistance * warpDistance || (!ridden && d > idleDistance * idleDistance)) {
            Place(s, pos);
            return;
        }

        // ridden: run after the partner like a whistled horse, at most once per second
        const auto now = std::chrono::steady_clock::now();
        if (ridden && d > callDistance * callDistance && now - lastCall >= std::chrono::seconds(1)) {
            lastCall = now;
            uint64_t message[4]{whistleMessage, partnerIndex, 0, 0};
            uint64_t result[2]{};
            MessageHook(s, result, 1, message);
        }
    }

    void Hide() {
        if (!active) {
            return;
        }
        active = false;
        spdlog::info("partner horse: partner has no horse, hiding");
        if (const auto s = horseType.load()) {
            Place(s, Vector3{.x = 0, .y = 0.5f, .z = 0}); // where unused horses wait
        }
    }
} // namespace Dynamite::PartnerHorse
