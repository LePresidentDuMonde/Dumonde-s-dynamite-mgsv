#include "DynamiteLua.h"
#include "DynamiteHook.h"
#include "dynamite.h"
#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "PartnerHorse.h"
#include "memtag.h"
#include "spdlog/spdlog.h"
#include <cstring>

namespace Dynamite {
    // TppMain.lua, called automatically on mission start
    int l_CreateHostSession(lua_State *L) {
        spdlog::info(__PRETTY_FUNCTION__);
        if (!g_hook->cfg.Host) {
            spdlog::info("{}, client, refusing to create host session", __PRETTY_FUNCTION__);

            return 0;
        }

        if (hookState.fobTargetCtor == nullptr) {
            spdlog::info("{}, fob target ctor is null", __PRETTY_FUNCTION__);

            return 0;
        }

        if (g_hook->dynamiteCore.GetHostSessionCreated()) {
            lua_getglobal(L, "TppUiCommand");
            lua_getfield(L, -1, "AnnounceLogViewLangId");
            const auto text = "dynamite_created_host_session\0";
            lua_pushstring(L, text);
            lua_pcall(L, 1, 0, 0);

            spdlog::info("{}, host session already created", __PRETTY_FUNCTION__);

            return 0;
        }

        auto res = CreateHostSession((FobTarget *)hookState.fobTargetCtor);
        spdlog::info("{}, host session res: {}", __PRETTY_FUNCTION__, res);

        g_hook->dynamiteCore.SetHostSessionCreated(res > 0);

        lua_getglobal(L, "TppUiCommand");
        lua_getfield(L, -1, "AnnounceLogViewLangId");
        const auto text = "dynamite_created_host_session\0";
        lua_pushstring(L, text);
        lua_pcall(L, 1, 0, 0);

        return 0;
    }

    // allow client to create session again
    // called manually
    int l_ResetClientSessionStateWithNotification(lua_State *L) {
        spdlog::info(__PRETTY_FUNCTION__);
        l_ResetClientSessionState(L);

        lua_getglobal(L, "TppUiCommand");
        lua_getfield(L, -1, "AnnounceLogViewLangId");
        const auto text = "dynamite_session_status_reset\0";
        lua_pushstring(L, text);
        lua_pcall(L, 1, 0, 0);

        return 0;
    }

    // allow client to create session again
    // called on mission end
    int l_ResetClientSessionState(lua_State *L) {
        spdlog::info(__PRETTY_FUNCTION__);

        g_hook->dynamiteCore.ResetState();
        g_hook->dynamiteSyncImpl.Stop();

        return 0;
    }

    // called manually by client
    int l_CreateClientSession(lua_State *L) {
        spdlog::info(__PRETTY_FUNCTION__);
        if (g_hook->cfg.Host) {
            const auto text = "dynamite_host_connect_attempt\0";
            spdlog::warn(text);
            lua_pushstring(L, text);
            TppUiUiCommandAnnounceLogViewLangId(L);

            return 1;
        }

        if (g_hook->dynamiteCore.GetSessionCreated()) {
            spdlog::info("{}, session already created", __PRETTY_FUNCTION__);

            return 0;
        }

        // fob target and session connect info can contain any data, preferably nulls
        // targetIPCString must be set to anything > 0, crash otherwise
        // 3 just works
        // real target ip is retrieved later by Steam

        // WARNING: TODO: these allocations will be made on current block
        // it works only because client clears that memory by disconnecting on mission end
        // see issue_7.md for details

        const auto ff = (FobTarget *)BlockHeapAlloc(sizeof(FobTarget), 8, MEMTAG_NULL);
        const auto connectInfo = (SessionConnectInfo *)BlockHeapAlloc(sizeof(SessionConnectInfo), 8, MEMTAG_NULL);
        if (ff == nullptr || connectInfo == nullptr) {
            spdlog::error("{}, block heap alloc failed", __PRETTY_FUNCTION__);
            if (ff) BlockHeapFree(ff);
            if (connectInfo) BlockHeapFree(connectInfo);
            return 0;
        }

        memset(ff, 0, sizeof(FobTarget));
        memset(connectInfo, 0, sizeof(SessionConnectInfo));
        static char test[] = {3, 0};
        ff->sessionConnectInfo = connectInfo;
        ff->sessionConnectInfo->targetIPCString = test;
        const auto res = CreateClientSession(ff, connectInfo);
        BlockHeapFree(ff);
        BlockHeapFree(connectInfo);

        auto text = "dynamite_establishing_connection\0";

        if (res == 1) {
            spdlog::info("{}, host {:d}", text, g_hook->cfg.HostSteamID);
            lua_pushstring(L, text);
            TppUiUiCommandAnnounceLogViewLangId(L);
            g_hook->dynamiteCore.SetSessionCreated(res);

            return 1;
        }

        text = "dynamite_connection_failed\0";
        spdlog::error("{}, game refused to create client session (res={})", text, res);

        lua_pushstring(L, text);
        TppUiUiCommandAnnounceLogViewLangId(L);

        g_hook->dynamiteCore.SetSessionCreated(res);

        return 1;
    }

    // *_sequence.lua, called on mission start
    int l_StartNearestEnemyThread(lua_State *L) {
        spdlog::info(__PRETTY_FUNCTION__);
        g_hook->dynamiteCore.StartNearestEnemyThread();

        return 0;
    }

    // TppMain, TppMission, called on mission end
    int l_StopNearestEnemyThread(lua_State *L) {
        spdlog::info(__PRETTY_FUNCTION__);
        g_hook->dynamiteCore.StopNearestEnemyThread();

        return 0;
    }

    // not used in lua, but exposed just in case
    int l_IsNearestEnemyThreadRunning(lua_State *L) {
        lua_pushboolean(L, g_hook->dynamiteCore.IsNearestEnemyThreadRunning());

        return 1;
    }

    // not used in lua, but exposed just in case
    int l_GetOffensePlayerIndex(lua_State *L) {
        lua_pushinteger(L, g_hook->dynamiteCore.GetOffensePlayerID());

        return 1;
    }

    // not used in lua, but exposed just in case
    int l_GetDefensePlayerIndex(lua_State *L) {
        lua_pushinteger(L, g_hook->dynamiteCore.GetDefensePlayerID());

        return 1;
    }

    // not used in lua, but exposed just in case
    int l_GetSoldierLifeStatus(lua_State *L) {
        const int oid = luaL_checkinteger(L, 1);
        const auto res = g_hook->dynamiteCore.GetSoldierLifeStatus(oid);
        lua_pushinteger(L, res);

        return 1;
    }

    // called in TppMain to set IS_ONLINE flag
    int l_IsHost(lua_State *L) {
        lua_pushboolean(L, g_hook->cfg.Host);

        return 1;
    }

    int l_IsClient(lua_State *L) {
        lua_pushboolean(L, !g_hook->cfg.Host);

        return 1;
    }

    int l_AddFixedMarker(lua_State *L) {
        const auto x = luaL_checknumber(L, 1);
        const auto y = luaL_checknumber(L, 2);
        const auto z = luaL_checknumber(L, 3);
        auto vv = Vector3{
            .x = static_cast<float>(x),
            .y = static_cast<float>(y),
            .z = static_cast<float>(z),
        };

        Marker2SystemImplPlacedUserMarkerFixed(hookState.markerSystemImpl, &vv);

        return 0;
    }

    int l_AddObjectMarker(lua_State *L) {
        const auto x = luaL_checknumber(L, 1);
        const auto y = luaL_checknumber(L, 2);
        const auto z = luaL_checknumber(L, 3);
        const auto id = luaL_checknumber(L, 4);
        auto vv = Vector3{
            .x = static_cast<float>(x),
            .y = static_cast<float>(y),
            .z = static_cast<float>(z),
        };

        auto ok = Marker2SystemImplPlacedUserMarkerFollow(hookState.markerSystemImpl, &vv, id);
        return 0;
    }

    // Dynamite.SendCarryDrop(gameObjectId, x, y, z, rotY)
    int l_SendCarryDrop(lua_State *L) {
        const auto id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
        const auto pos = Vector3{
            .x = static_cast<float>(luaL_checknumber(L, 2)),
            .y = static_cast<float>(luaL_checknumber(L, 3)),
            .z = static_cast<float>(luaL_checknumber(L, 4)),
        };
        g_hook->dynamiteSyncImpl.SendCarryDrop(id, &pos, static_cast<float>(luaL_checknumber(L, 5)));

        return 0;
    }

    // returns gameObjectId, x, y, z, rotY of the next npc put down by partner, nothing if there is none
    int l_PopCarryDrop(lua_State *L) {
        const auto drop = g_hook->dynamiteSyncImpl.PopCarryDrop();
        if (!drop) {
            return 0;
        }

        lua_pushinteger(L, drop->objectID);
        lua_pushnumber(L, drop->pos.x);
        lua_pushnumber(L, drop->pos.y);
        lua_pushnumber(L, drop->pos.z);
        lua_pushnumber(L, drop->rotY);

        return 5;
    }

    // Dynamite.SendVehicleState(gameObjectId, x, y, z, rotY, hasRot, driven), throttled
    int l_SendVehicleState(lua_State *L) {
        g_hook->dynamiteSyncImpl.SendVehicleState({
            .objectID = static_cast<uint32_t>(luaL_checkinteger(L, 1)),
            .pos =
                {
                    .x = static_cast<float>(luaL_checknumber(L, 2)),
                    .y = static_cast<float>(luaL_checknumber(L, 3)),
                    .z = static_cast<float>(luaL_checknumber(L, 4)),
                },
            .rotY = static_cast<float>(luaL_checknumber(L, 5)),
            .hasRot = lua_toboolean(L, 6) != 0,
            .driven = lua_toboolean(L, 7) != 0,
        });

        return 0;
    }

    // returns gameObjectId, x, y, z, rotY, hasRot, driven of the next vehicle update from partner, nothing if there is none
    int l_PopVehicleState(lua_State *L) {
        const auto v = g_hook->dynamiteSyncImpl.PopVehicleState();
        if (!v) {
            return 0;
        }

        lua_pushinteger(L, v->objectID);
        lua_pushnumber(L, v->pos.x);
        lua_pushnumber(L, v->pos.y);
        lua_pushnumber(L, v->pos.z);
        lua_pushnumber(L, v->rotY);
        lua_pushboolean(L, v->hasRot);
        lua_pushboolean(L, v->driven);

        return 7;
    }

    int l_RemoveMarker(lua_State *L) {
        const auto i = luaL_checkinteger(L, 1);
        Marker2SystemImplRemovedUserMarker(hookState.markerSystemImpl, i);

        return 0;
    }

    int l_RemoveAllUserMarkers(lua_State *L) {
        spdlog::info("{}, removing all user markers", __PRETTY_FUNCTION__);
        Marker2SystemImplRemovedAllUserMarker(hookState.markerSystemImpl);

        return 0;
    }

    int l_IgnoreMarkerRequests(lua_State *L) {
        spdlog::info("{}, ignoring marker requests", __PRETTY_FUNCTION__);
        hookState.ignoreMarkerRequests = true;

        return 0;
    }

    int l_AcceptMarkerRequests(lua_State *L) {
        spdlog::info("{}, accepting marker requests", __PRETTY_FUNCTION__);
        hookState.ignoreMarkerRequests = false;

        return 0;
    }

    int l_GetPlayerPosition(lua_State *L) {
        auto i = luaL_checkinteger(L, 1);
        auto res = g_hook->dynamiteCore.GetPlayerPosition(i);
        auto rr = Vector4{
            .x = res.x,
            .y = res.y,
            .z = res.z,
            .w = 0,
        };
        spdlog::info("{}, {}: {} {} {}", __PRETTY_FUNCTION__, i, res.x, res.y, res.z);
        FoxLuaPushVector3(L, &rr);

        return 1;
    }

    int l_WarpToPartner(lua_State *L) {
        spdlog::info("{}", __PRETTY_FUNCTION__);
        const auto count = g_hook->dynamiteCore.GetMemberCount();
        if (count < 2) {
            spdlog::info("{}, no player to warp to ({} players)", __PRETTY_FUNCTION__, count);

            lua_getglobal(L, "TppUiCommand");
            lua_getfield(L, -1, "AnnounceLogViewLangId");
            const auto text = "dynamite_warp_no_player\0";
            lua_pushstring(L, text);
            lua_pcall(L, 1, 0, 0);

            return 0;
        }

        uint16_t partnerID = 0;
        if (g_hook->cfg.Host) {
            partnerID = 1;
        }

        auto partnerPos = g_hook->dynamiteCore.GetPlayerPosition(partnerID);

        Quat rot = {
            .x = 0,
            .y = -0.1f,
            .z = 0,
            .w = 7.0f,
        };

        auto playerID = 1;
        if (g_hook->cfg.Host) {
            playerID = 0;
        }

        auto gameObject = FindGameObjectWithID(playerID);
        if (gameObject == nullptr) {
            spdlog::info("{}, cannot find game object to warp", __PRETTY_FUNCTION__);
            return 0;
        }

        gameObject = (char *)gameObject + 0x20;

        spdlog::info(
            "{}, player {} warping to partner {}, position {}, {}, {}", __PRETTY_FUNCTION__, playerID, partnerID, partnerPos.x, partnerPos.y, partnerPos.z);

        Player2GameObjectImplWarp(gameObject, playerID, &partnerPos, &rot, true);

        return 0;
    }

    int l_IsSynchronized(lua_State *L) {
        const auto res = g_hook->dynamiteSyncImpl.IsSynchronized();
        lua_pushboolean(L, res);
        return 1;
    }

    int l_RequestVar(lua_State *L) {
        size_t len;
        const auto name = luaL_checklstring(L, 2, &len);
        if (len < 1) {
            spdlog::error("{}, name is empty", __PRETTY_FUNCTION__);
            return 0;
        }

        const auto category = luaL_checklstring(L, 1, &len);
        if (len < 1) {
            spdlog::error("{}, category is empty", __PRETTY_FUNCTION__);
            return 0;
        }

        g_hook->dynamiteSyncImpl.RequestVar(category, name);
        return 0;
    }

    int l_Ping(lua_State *L) {
        g_hook->dynamiteSyncImpl.Ping();
        return 0;
    }

    // returns x, y, z of the local buddy horse, nothing if unknown yet
    int l_GetLocalHorse(lua_State *L) {
        const auto pos = PartnerHorse::LocalHorse();
        if (!pos) {
            return 0;
        }
        lua_pushnumber(L, pos->x);
        lua_pushnumber(L, pos->y);
        lua_pushnumber(L, pos->z);
        return 3;
    }

    // Dynamite.FollowPartnerHorse(x, y, z, ridden): drives the partner horse copy to partner's horse
    int l_FollowPartnerHorse(lua_State *L) {
        const auto pos = Vector3{.x = static_cast<float>(luaL_checknumber(L, 1)), .y = static_cast<float>(luaL_checknumber(L, 2)), .z = static_cast<float>(luaL_checknumber(L, 3))};
        PartnerHorse::Follow(pos, lua_toboolean(L, 4) != 0, g_hook->cfg.Host ? 1 : 0);
        return 0;
    }

    int l_HidePartnerHorse(lua_State *L) {
        PartnerHorse::Hide();
        return 0;
    }

    int l_Log(lua_State *L) {
        size_t len;
        const auto str = luaL_checklstring(L, 1, &len);
        if (len < 1) {
            spdlog::error("{}, str is empty", __PRETTY_FUNCTION__);
            return 0;
        }

        spdlog::info("[lua] {}", str);

        return 0;
    }

    int l_Donated(lua_State *L) {
        lua_pushboolean(L, g_hook->cfg.donated);

        return 1;
    }

    int l_MissionComplete(lua_State *L) {
        g_hook->dynamiteCore.MissionComplete();

        return 0;
    }

    int l_GetMissionsCompleted(lua_State *L) {
        lua_pushinteger(L, g_hook->dynamiteCore.GetMissionsCompleted());

        return 1;
    }

    int l_GetActiveEquipmentID(lua_State *L) {
        g_hook->dynamiteCore.GetActiveEquipmentID(0);

        return 0;
    }

    int l_GetEquipIDInSlot(lua_State *L) {
        const int playerID = luaL_checkinteger(L, 1);
        const int slot = luaL_checkinteger(L, 2);
        const int index = luaL_checkinteger(L, 3);

        const auto res = g_hook->dynamiteCore.GetEquipIDInSlot(playerID, slot, index);

        spdlog::info("{}, playerID={}, slot={}, index={}, equipID={}", __PRETTY_FUNCTION__, playerID, slot, index, res);

        lua_pushinteger(L, res);

        return 1;
    }

    // int l_GetCamoRate(lua_State *L) {
    //     // wrong!
    //     // auto lvar8 = *(uint32_t *)((char *)camouflageControllerImpl + 0x38);
    //     // auto res = *(float *)lvar8;
    //     // lua_pushnumber(L, res);
    //     return 0;
    // }

    void CreateLibs(lua_State *L) {
        luaL_Reg libFuncs[] = {
            {"CreateHostSession", l_CreateHostSession},
            {"CreateClientSession", l_CreateClientSession},
            {"ResetClientSessionState", l_ResetClientSessionState},
            {"ResetClientSessionStateWithNotification", l_ResetClientSessionStateWithNotification},
            {"StartNearestEnemyThread", l_StartNearestEnemyThread},
            {"StopNearestEnemyThread", l_StopNearestEnemyThread},
            {"IsNearestEnemyThreadRunning", l_IsNearestEnemyThreadRunning},
            {"GetOffensePlayerIndex", l_GetOffensePlayerIndex},
            {"GetDefensePlayerIndex", l_GetDefensePlayerIndex},
            {"GetSoldierLifeStatus", l_GetSoldierLifeStatus},
            {"AddFixedMarker", l_AddFixedMarker},
            {"AddObjectMarker", l_AddObjectMarker},
            {"RemoveMarker", l_RemoveMarker},
            {"SendCarryDrop", l_SendCarryDrop},
            {"PopCarryDrop", l_PopCarryDrop},
            {"SendVehicleState", l_SendVehicleState},
            {"PopVehicleState", l_PopVehicleState},
            {"RemoveAllUserMarkers", l_RemoveAllUserMarkers},
            {"IgnoreMarkerRequests", l_IgnoreMarkerRequests},
            {"AcceptMarkerRequests", l_AcceptMarkerRequests},
            {"IsClient", l_IsClient},
            {"IsHost", l_IsHost},
            {"GetPlayerPosition", l_GetPlayerPosition},
            {"WarpToPartner", l_WarpToPartner},
            {"IsSynchronized", l_IsSynchronized},
            {"RequestVar", l_RequestVar},
            {"Ping", l_Ping},
            {"Log", l_Log},
            {"GetLocalHorse", l_GetLocalHorse},
            {"FollowPartnerHorse", l_FollowPartnerHorse},
            {"HidePartnerHorse", l_HidePartnerHorse},
            {"Donated", l_Donated},
            {"MissionComplete", l_MissionComplete},
            {"GetMissionsCompleted", l_GetMissionsCompleted},
            {"GetEquipment", l_GetActiveEquipmentID},
            {"GetEquipIDInSlot", l_GetEquipIDInSlot},
            {nullptr, nullptr},
        };
        luaI_openlib(L, "Dynamite", libFuncs, 0);
    }
}