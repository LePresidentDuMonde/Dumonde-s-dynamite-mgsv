#include "dynamite.h"
#include "mgsvtpp_func_typedefs.h"

namespace Dynamite {
    void Dynamite::SetFuncPtrs() {
        luaI_openlib = (luaI_openlibFunc *)AddressOf("luaI_openlib");
        luaL_openlibs = (luaL_openlibsFunc *)AddressOf("luaL_openlibs");

        lua_pushinteger = (lua_pushintegerFunc *)AddressOf("lua_pushinteger");
        lua_setfield = (lua_setfieldFunc *)AddressOf("lua_setfield");
        lua_pushstring = (lua_pushstringFunc *)AddressOf("lua_pushstring");
        lua_getfield = (lua_getfieldFunc *)AddressOf("lua_getfield");
        lua_pcall = (lua_pcallFunc *)AddressOf("lua_pcall");
        luaL_checkinteger = (luaL_checkintegerFunc *)AddressOf("luaL_checkinteger");
        lua_pushboolean = (lua_pushbooleanFunc *)AddressOf("lua_pushboolean");
        luaL_checknumber = (luaL_checknumberFunc *)AddressOf("luaL_checknumber");
        lua_toboolean = (lua_tobooleanFunc *)AddressOf("lua_toboolean");
        lua_createtable = (lua_createtableFunc *)AddressOf("lua_createtable");
        lua_pushvalue = (lua_pushvalueFunc *)AddressOf("lua_pushvalue");
        luaL_checklstring = (luaL_checklstringFunc *)AddressOf("luaL_checklstring");
        lua_pushnumber = (lua_pushnumberFunc *)AddressOf("lua_pushnumber");

        IsDefenseTeamByOnlineFobLocal = (IsDefenseTeamByOnlineFobLocalFunc *)AddressOf("IsDefenseTeamByOnlineFobLocal");
        IsOffenseTeamByOnlineFobLocal = (IsOffenseTeamByOnlineFobLocalFunc *)AddressOf("IsOffenseTeamByOnlineFobLocal");
        CreateHostSession = (CreateHostSessionFunc *)AddressOf("CreateHostSession");
        CreateClientSession = (CreateClientSessionFunc *)AddressOf("CreateClientSession");
        SetSteamId = (SetSteamIdFunc *)AddressOf("SetSteamId");
        FobTargetCtor = (FobTargetCtorFunc *)AddressOf("FobTargetCtor");
        SessionConnectInfoCtor = (SessionConnectInfoCtorFunc *)AddressOf("SessionConnectInfoCtor");
        NetworkSystemCreateHostSession = (NetworkSystemCreateHostSessionFunc *)AddressOf("NetworkSystemCreateHostSession");
        BlockHeapAlloc = (BlockHeapAllocFunc *)AddressOf("BlockHeapAlloc");
        BlockHeapFree = (BlockHeapFreeFunc *)AddressOf("BlockHeapFree");
        NetInfoCreateSession = (NetInfoCreateSessionFunc *)AddressOf("NetInfoCreateSession");
        IsConnectReady = (IsConnectReadyFunc *)AddressOf("IsConnectReady");
        UpdateClientEstablished = (UpdateClientEstablishedFunc *)AddressOf("UpdateClientEstablished");
        GetQuarkSystemTable = (GetQuarkSystemTableFunc *)AddressOf("GetQuarkSystemTable");
        SessionImpl2IsHost = (SessionImpl2IsHostFunc *)AddressOf("SessionImpl2IsHost");
        GetFobOffensePlayerInstanceIndex = (GetFobOffensePlayerInstanceIndexFunc *)AddressOf("GetFobOffensePlayerInstanceIndex");
        GetFobDefensePlayerInstanceIndex = (GetFobDefensePlayerInstanceIndexFunc *)AddressOf("GetFobDefensePlayerInstanceIndex");
        HashMapFindKey = (HashMapOperatorFindKeyFunc *)AddressOf("HashMapFindKey");
        FoxStrHash32 = (FoxStrHash32Func *)AddressOf("FoxStrHash32");
        CommandPost2ImplGetAvailableMemberCount = (CommandPost2ImplGetAvailableMemberCountFunc *)AddressOf("CommandPost2ImplGetAvailableMemberCount");
        CommandPost2ImplGetMaxCpCount = (CommandPost2ImplGetMaxCpCountFunc *)AddressOf("CommandPost2ImplGetMaxCpCount");
        FoxFindEntity = (FoxFindEntityFunc *)AddressOf("FoxFindEntity");
        FoxGetEntityHandle = (FoxGetEntityHandleFunc *)AddressOf("FoxGetEntityHandle");
        FoxGetEntityProperty = (FoxGetEntityPropertyFunc *)AddressOf("FoxGetEntityProperty");
        FoxCreatePropertyInfo = (FoxCreatePropertyInfoFunc *)AddressOf("FoxCreatePropertyInfo");
        ValueGetAsUInt32 = (ValueGetAsUInt32Func *)AddressOf("ValueGetAsUInt32");
        FoxString = (FoxStringFunc *)AddressOf("FoxString");
        FindGameObjectWithID = (FindGameObjectWithIDFunc *)AddressOf("FindGameObjectWithID");
        Soldier2ImplGetInterface = (Soldier2ImplGetInterfaceFunc *)AddressOf("Soldier2ImplGetInterface");
        PlayerInfoServiceGetPositionAtIndex = (PlayerInfoServiceGetPositionAtIndexFunc *)AddressOf("PlayerInfoServiceGetPositionAtIndex");
        FoxNtSessionGetMainSession = (FoxNtSessionGetMainSessionFunc *)AddressOf("FoxNtSessionGetMainSession");
        GetSessionMemberCount = (GetSessionMemberCountFunc *)AddressOf("GetSessionMemberCount");
        GetUiMarkerTypeFromSystemType2 = (GetUiMarkerTypeFromSystemType2Func *)AddressOf("GetUiMarkerTypeFromSystemType2");
        SightManagerImplSetMarker = (SightManagerImplSetMarkerFunc *)AddressOf("SightManagerImplSetMarker");
        SteamUdpSocketImplOnP2PSessionRequest = (SteamUdpSocketImplOnP2PSessionRequestFunc *)AddressOf("SteamUdpSocketImplOnP2PSessionRequest");
        MessageBufferAddMessage = (MessageBufferAddMessageFunc *)AddressOf("MessageBufferAddMessage");
        AddLocalDamage = (AddLocalDamageFunc *)AddressOf("DamageControllerImplAddLocalDamage");
        DamageControllerImplInitialize = (DamageControllerImplInitializeFunc *)AddressOf("DamageControllerImplInitialize");
        SynchronizerImplGetDamage = (SynchronizerImplGetDamageFunc *)AddressOf("SynchronizerImplGetDamage");
        Marker2SystemImpl = (Marker2SystemImplFunc *)AddressOf("Marker2SystemImpl");
        Marker2SystemImplPlacedUserMarkerFixed = (Marker2SystemImplPlacedUserMarkerFixedFunc *)AddressOf("Marker2SystemImplPlacedUserMarkerFixed");
        GetLocalPlayerId = (GetLocalPlayerIDFunc *)AddressOf("GetLocalPlayerID");
        RemoveUserMarker = (RemoveUserMarkerFunc *)AddressOf("RemoveUserMarker");
        EquipHudSystemImplInitData = (EquipHudSystemImplInitDataFunc *)AddressOf("EquipHudSystemImplInitData");
        Marker2SystemImplRemovedUserMarker = (Marker2SystemImplRemovedUserMarkerFunc *)AddressOf("Marker2SystemImplRemovedUserMarker");
        Marker2SystemImplPlacedUserMarkerFollow = (Marker2SystemImplPlacedUserMarkerFollowFunc *)AddressOf("Marker2SystemImplPlacedUserMarkerFollow");
        Marker2SystemImplRemovedAllUserMarker = (Marker2SystemImplRemovedAllUserMarkerFunc *)AddressOf("Marker2SystemImplRemovedAllUserMarker");
        SightManagerImplInitialize = (SightManagerImplInitializeFunc *)AddressOf("SightManagerImplInitialize");
        UiControllerImplSetNoUseEquipId = (UiControllerImplSetNoUseEquipIdFunc *)AddressOf("UiControllerImplSetNoUseEquipId");
        EquipCrossEvCallIsItemNoUse = (EquipCrossEvCallIsItemNoUseFunc *)AddressOf("EquipCrossEvCallIsItemNoUse");
        FoxBlockProcess = (FoxBlockProcessFunc *)AddressOf("FoxBlockProcess");
        FoxBlockUnload = (FoxBlockUnloadFunc *)AddressOf("FoxBlockUnload");
        FoxBlockReload = (FoxBlockReloadFunc *)AddressOf("FoxBlockReload");
        FoxGenerateUniqueName = (FoxGenerateUniqueNameFunc *)AddressOf("FoxGenerateUniqueName");
        FoxBlock = (FoxBlockFunc *)AddressOf("FoxBlock");
        GetCurrentBlockMemory = (GetCurrentBlockMemoryFunc *)AddressOf("GetCurrentBlockMemory");
        BlockMemoryAllocTail = (BlockMemoryAllocTailFunc *)AddressOf("BlockMemoryAllocTail");
        BlockMemoryAllocHeap = (BlockMemoryAllocHeapFunc *)AddressOf("BlockMemoryAllocHeap");
        FoxBlockActivate = (FoxBlockActivateFunc *)AddressOf("FoxBlockActivate");
        FoxBlockDeactivate = (FoxBlockDeactivateFunc *)AddressOf("FoxBlockDeactivate");
        FoxBlockLoad = (FoxBlockLoadFunc *)AddressOf("FoxBlockLoad");
        CloseSession = (CloseSessionFunc *)AddressOf("CloseSession");
        Player2GameObjectImplWarp = (Player2GameObjectImplWarpFunc *)AddressOf("Player2GameObjectImplWarp");
        ScriptDeclVarsImplSetVarValue = (ScriptDeclVarsImplSetVarValueFunc *)AddressOf("ScriptDeclVarsImplSetVarValue");
        SoldierRouteAiImplPreUpdate = (SoldierRouteAiImplPreUpdateFunc *)AddressOf("SoldierRouteAiImplPreUpdate");
        RouteGroupImplGetEventId = (RouteGroupImplGetEventIdFunc *)AddressOf("RouteGroupImplGetEventId");
        FoxCreateQuark = (FoxCreateQuarkFunc *)AddressOf("FoxCreateQuark");
        AiControllerImplAddNode = (AiControllerImplAddNodeFunc *)AddressOf("AiControllerImplAddNode");
        AiControllerImplSleepNode = (AiControllerImplSleepNodeFunc *)AddressOf("AiControllerImplSleepNode");
        AiControllerImplWakeNode = (AiControllerImplWakeNodeFunc *)AddressOf("AiControllerImplWakeNode");
        CoreAiImplGetVehicleId = (CoreAiImplGetVehicleIdFunc *)AddressOf("CoreAiImplGetVehicleId");
        CoreAiImplGetVehicleRideState = (CoreAiImplGetVehicleRideStateFunc *)AddressOf("CoreAiImplGetVehicleRideState");
        CoreAiImplIsVehicleRetain = (CoreAiImplIsVehicleRetainFunc *)AddressOf("CoreAiImplIsVehicleRetain");
        SoldierImplRouteAiImplCheckVehicleAndWalkerGearGetInAndOutStep =
            (SoldierImplRouteAiImplCheckVehicleAndWalkerGearGetInAndOutStepFunc *)AddressOf("SoldierImplRouteAiImplCheckVehicleAndWalkerGearGetInAndOutStep");
        StatusControllerImplIsSet = (StatusControllerImplIsSetFunc *)AddressOf("StatusControllerImplIsSet");
        ScriptDeclVarsImplGetVarHandleWithVarIndex = (ScriptDeclVarsImplGetVarHandleWithVarIndexFunc *)AddressOf("ScriptDeclVarsImplGetVarHandleWithVarIndex");
        GmGetScriptVarInfo = (GmGetScriptVarInfoFunc *)AddressOf("GmGetScriptVarInfo");
        BandWidthManagerImplCalcAverageRttOfBetterHalfConnection =
            (BandWidthManagerImplCalcAverageRttOfBetterHalfConnectionFunc *)AddressOf("BandWidthManagerImplCalcAverageRttOfBetterHalfConnection");
        BandWidthManagerImplCalcAverageLostRateOfBetterHalfConnection =
            (BandWidthManagerImplCalcAverageLostRateOfBetterHalfConnectionFunc *)AddressOf("BandWidthManagerImplCalcAverageLostRateOfBetterHalfConnection");
        BandWidthManagerImplStartLimitState = (BandWidthManagerImplStartLimitStateFunc *)AddressOf("BandWidthManagerImplStartLimitState");

        l_AnnounceLogView = (lua_CFunction)AddressOf("l_AnnounceLogView");
        TppUiUiCommandAnnounceLogViewLangId = (lua_CFunction)AddressOf("TppUiUiCommandAnnounceLogViewLangId");
        FoxLuaPushVector3 = (FoxLuaPushVector3Func *)AddressOf("FoxLuaPushVector3");
        FoxLuaPushQuat = (FoxLuaPushQuatFunc *)AddressOf("FoxLuaPushQuat");
        FoxNioMpMessageContainerGetFreeSize = (FoxNioMpMessageContainerGetFreeSizeFunc *)AddressOf("FoxNioMpMessageContainerGetFreeSize");
        FoxNioImplMpMuxImplSend = (FoxNioImplMpMuxImplSendFunc *)AddressOf("FoxNioImplMpMuxImplSend");
        FoxNioImplMpMuxImplRecv1 = (FoxNioImplMpMuxImplRecv1Func *)AddressOf("FoxNioImplMpMuxImplRecv1");
        FoxNioImplMpMuxImplRecv2 = (FoxNioImplMpMuxImplRecv2Func *)AddressOf("FoxNioImplMpMuxImplRecv2");
        FoxNtPeerControllerSend = (FoxNtPeerControllerSendFunc *)AddressOf("FoxNtPeerControllerSend");
        FoxNtImplGameSocketImplPeerIsSendPacketEmpty =
            (FoxNtImplGameSocketImplPeerIsSendPacketEmptyFunc *)AddressOf("FoxNtImplGameSocketImplPeerIsSendPacketEmpty");
        FoxNtTotalControllerSend = (FoxNtTotalControllerSendFunc *)AddressOf("FoxNtTotalControllerSend");
        FoxNtImplTransceiverManagerImplPeerSend = (FoxNtImplTransceiverManagerImplPeerSendFunc *)AddressOf("FoxNtImplTransceiverManagerImplPeerSend");
        FoxNioImplMpSocketImplSend = (FoxNioImplMpSocketImplSendFunc *)AddressOf("FoxNioImplMpSocketImplSend");
        FoxNioImplMpMuxImplGetTotalPayloadSize = (FoxNioImplMpMuxImplGetTotalPayloadSizeFunc *)AddressOf("FoxNioImplMpMuxImplGetTotalPayloadSize");
        FoxNioMpMessageSerializerSerialize = (FoxNioMpMessageSerializerSerializeFunc *)AddressOf("FoxNioMpMessageSerializerSerialize");
        FoxNioMpMessageContainerCreate = (FoxNioMpMessageContainerCreateFunc *)AddressOf("FoxNioMpMessageContainerCreate");
        FoxNioMpMessageContainerAddMessage = (FoxNioMpMessageContainerAddMessageFunc *)AddressOf("FoxNioMpMessageContainerAddMessage");
        FoxNioImplSppSocketImplGetState = (FoxNioImplSppSocketImplGetStateFunc *)AddressOf("FoxNioImplSppSocketImplGetState");
        FoxNtImplSyncMemoryCollectorSyncMemoryCollector =
            (FoxNtImplSyncMemoryCollectorSyncMemoryCollectorFunc *)AddressOf("FoxNtImplSyncMemoryCollectorSyncMemoryCollector");
        FoxNtImplGameSocketBufferImplAlloc = (FoxNtImplGameSocketBufferImplAllocFunc *)AddressOf("FoxNtImplGameSocketBufferImplAlloc");
        TppGmImplScriptDeclVarsImplOnSessionNotify = (TppGmImplScriptDeclVarsImplOnSessionNotifyFunc *)AddressOf("TppGmImplScriptDeclVarsImplOnSessionNotify");
        FoxBitStreamWriterPrimitiveWrite = (FoxBitStreamWriterPrimitiveWriteFunc *)AddressOf("FoxBitStreamWriterPrimitiveWrite");
        TppGmImplScriptDeclVarsImplGetVarHandleWithVarIndex =
            (TppGmImplScriptDeclVarsImplGetVarHandleWithVarIndexFunc *)AddressOf("TppGmImplScriptDeclVarsImplGetVarHandleWithVarIndex");
        FoxNtImplTransceiverManagerImplPeerSendImpl1 =
            (FoxNtImplTransceiverManagerImplPeerSendImpl1Func *)AddressOf("FoxNtImplTransceiverManagerImplPeerSendImpl1");
        FoxNtImplTransceiverManagerImplPeerSendImpl2 =
            (FoxNtImplTransceiverManagerImplPeerSendImpl2Func *)AddressOf("FoxNtImplTransceiverManagerImplPeerSendImpl2");
        FoxNtImplTransceiverImplTransceiverImpl = (FoxNtImplTransceiverImplTransceiverImplFunc *)AddressOf("FoxNtImplTransceiverImplTransceiverImpl");
        FoxNtImplGameSocketImplPeerRequestToSend = (FoxNtImplGameSocketImplPeerRequestToSendFunc *)AddressOf("FoxNtImplGameSocketImplPeerRequestToSend");
        FoxNtImplGameSocketBufferImplGameSocketBufferImpl =
            (FoxNtImplGameSocketBufferImplGameSocketBufferImplFunc *)AddressOf("FoxNtImplGameSocketBufferImplGameSocketBufferImpl");
        FoxNioMpMessageCreate = (FoxNioMpMessageCreateFunc *)AddressOf("FoxNioMpMessageCreate");
        FoxNtImplNetworkSystemImplGetMainSession = (FoxNtImplNetworkSystemImplGetMainSessionFunc *)AddressOf("FoxNtImplNetworkSystemImplGetMainSession");
        FoxImplScopeAllocatorImplAlloc = (FoxImplScopeAllocatorImplAllocFunc *)AddressOf("FoxImplScopeAllocatorImplAlloc");
        FoxScopeMemoryScopeMemory1 = (FoxScopeMemoryScopeMemory1Func *)AddressOf("FoxScopeMemoryScopeMemory1");
        FoxScopeMemoryScopeMemoryDtor = (FoxScopeMemoryScopeMemoryDtorFunc *)AddressOf("FoxScopeMemoryScopeMemoryDtor");
        TppGmImplScriptDeclVarsImplUpdate = (TppGmImplScriptDeclVarsImplUpdateFunc *)AddressOf("TppGmImplScriptDeclVarsImplUpdate");
        FoxNtImplGameSocketImplGetPacketCount = (FoxNtImplGameSocketImplGetPacketCountFunc *)AddressOf("FoxNtImplGameSocketImplGetPacketCount");
        FoxNtImplGameSocketImplGetPacket = (FoxNtImplGameSocketImplGetPacketFunc *)AddressOf("FoxNtImplGameSocketImplGetPacket");
        FoxNtImplNetworkSystemImplCreateGameSocket = (FoxNtImplNetworkSystemImplCreateGameSocketFunc *)AddressOf("FoxNtImplNetworkSystemImplCreateGameSocket");
        FoxNtNtModuleInit = (FoxNtNtModuleInitFunc *)AddressOf("FoxNtNtModuleInit");
        FoxNtImplGameSocketImplRequestToSendToMember =
            (FoxNtImplGameSocketImplRequestToSendToMemberFunc *)AddressOf("FoxNtImplGameSocketImplRequestToSendToMember");
        FoxNtImplGameSocketImplSetInterval = (FoxNtImplGameSocketImplSetIntervalFunc *)AddressOf("FoxNtImplGameSocketImplSetInterval");
        FoxNtImplPeerCommonInitializeLastSendTime = (FoxNtImplPeerCommonInitializeLastSendTimeFunc *)AddressOf("FoxNtImplPeerCommonInitializeLastSendTime");
        FoxNtImplGameSocketImplGetPacketSize = (FoxNtImplGameSocketImplGetPacketSizeFunc *)AddressOf("FoxNtImplGameSocketImplGetPacketSize");
        FoxNtImplGameSocketImplGameSocketImplDtor = (FoxNtImplGameSocketImplGameSocketImplDtorFunc *)AddressOf("FoxNtImplGameSocketImplGameSocketImplDtor");
        TppGmImplScriptDeclVarsImplGetVarHandle = (TppGmImplScriptDeclVarsImplGetVarHandleFunc *)AddressOf("TppGmImplScriptDeclVarsImplGetVarHandle");
        TppGmImplScriptDeclVarsImplScriptDeclVarsImpl =
            (TppGmImplScriptDeclVarsImplScriptDeclVarsImplFunc *)AddressOf("TppGmImplScriptDeclVarsImplScriptDeclVarsImpl");
        TppGmImplScriptSystemImplGetScriptDeclVarHandle =
            (TppGmImplScriptSystemImplGetScriptDeclVarHandleFunc *)AddressOf("TppGmImplScriptSystemImplGetScriptDeclVarHandle");
        TppGmImplScriptDeclVarsImplGetVarIndexWithName =
            (TppGmImplScriptDeclVarsImplGetVarIndexWithNameFunc *)AddressOf("TppGmImplScriptDeclVarsImplGetVarIndexWithName");
        TppGmGetSVars = (TppGmGetSVarsFunc *)AddressOf("TppGmGetSVars");
        TppGmGetGVars = (TppGmGetGVarsFunc *)AddressOf("TppGmGetGVars");
        FoxNtImplSessionImpl2GetMemberInterfaceAtIndex =
            (FoxNtImplSessionImpl2GetMemberInterfaceAtIndexFunc *)AddressOf("FoxNtImplSessionImpl2GetMemberInterfaceAtIndex");
        FoxNtImplGameSocketImplHandleMessage = (FoxNtImplGameSocketImplHandleMessageFunc *)AddressOf("FoxNtImplGameSocketImplHandleMessage");
        FoxNtImplPeerCommonPeerCommon = (FoxNtImplPeerCommonPeerCommonFunc *)AddressOf("FoxNtImplPeerCommonPeerCommon");
        TppGmPlayerImplSynchronizerImplInitialize = (TppGmPlayerImplSynchronizerImplInitializeFunc *)AddressOf("TppGmPlayerImplSynchronizerImplInitialize");
        TppGmImplScriptDeclVarsImplScriptDeclVarsImplDtor =
            (TppGmImplScriptDeclVarsImplScriptDeclVarsImplDtorFunc *)AddressOf("TppGmImplScriptDeclVarsImplScriptDeclVarsDtor");
        TppGmPlayerImplAnonymous_namespaceCamouflageControllerImplInitialize = (TppGmPlayerImplAnonymous_namespaceCamouflageControllerImplInitializeFunc *)
            AddressOf("TppGmPlayerImplAnonymous_namespaceCamouflageControllerImplInitialize");
        FoxNtImplTransceiverManagerImplPeerAddToSendQueue =
            (FoxNtImplTransceiverManagerImplPeerAddToSendQueueFunc *)AddressOf("FoxNtImplTransceiverManagerImplPeerAddToSendQueue");
        FoxNioImplMpMuxImplSendUpdate = (FoxNioImplMpMuxImplSendUpdateFunc *)AddressOf("FoxNioImplMpMuxImplSendUpdate");
        FoxNioImplSppSocketImplSendImpl = (FoxNioImplSppSocketImplSendImplFunc *)AddressOf("FoxNioImplSppSocketImplSendImpl");
        FoxNioImplSteamUdpSocketImplSend = (FoxNioImplSteamUdpSocketImplSendFunc *)AddressOf("FoxNioImplSteamUdpSocketImplSend");
        FoxNioImplSteamUdpSocketImplRecv = (FoxNioImplSteamUdpSocketImplRecvFunc *)AddressOf("FoxNioImplSteamUdpSocketImplRecv");
        TppUiEmblemImplEmblemEditorSystemImplCreateEmblemParameters =
            (TppUiEmblemImplEmblemEditorSystemImplCreateEmblemParametersFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplCreateEmblemParameters");
        TppUiEmblemImplEmblemEditorSystemImplCreateEmblem =
            (TppUiEmblemImplEmblemEditorSystemImplCreateEmblemFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplCreateEmblem");
        TppUiEmblemImplEmblemEditorSystemImplLoadEmblemTextureInfo =
            (TppUiEmblemImplEmblemEditorSystemImplLoadEmblemTextureInfoFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplLoadEmblemTextureInfo");
        FoxNtImplSessionImpl2DeleteMember = (FoxNtImplSessionImpl2DeleteMemberFunc *)AddressOf("FoxNtImplSessionImpl2DeleteMember");
        FoxImplMessage2MessageBox2ImplSendMessageToSubscribers =
            (FoxImplMessage2MessageBox2ImplSendMessageToSubscribersFunc *)AddressOf("FoxImplMessage2MessageBox2ImplSendMessageToSubscribers");
        TppUiEmblemImplEmblemEditorSystemImplDeleteTexture =
            (TppUiEmblemImplEmblemEditorSystemImplDeleteTextureFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplDeleteTexture");
        TppUiEmblemImplEmblemEditorSystemImplUnloadEmblemTextureInfo =
            (TppUiEmblemImplEmblemEditorSystemImplUnloadEmblemTextureInfoFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplUnloadEmblemTextureInfo");
        TppGkTppGameKitModuleEnd = (TppGkTppGameKitModuleEndFunc *)AddressOf("TppGkTppGameKitModuleEnd");
        TppUiEmblemImplEmblemEditorSystemImplDeleteBlender =
            (TppUiEmblemImplEmblemEditorSystemImplDeleteBlenderFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplDeleteBlender");
        TppGkEmblemManagerCreateBlender = (TppGkEmblemManagerCreateBlenderFunc *)AddressOf("TppGkEmblemManagerCreateBlender");
        TppUiEmblemImplEmblemEditorSystemImplIsReady =
            (TppUiEmblemImplEmblemEditorSystemImplIsReadyFunc *)AddressOf("TppUiEmblemImplEmblemEditorSystemImplIsReady");
        TppGmPlayerImplClimbActionPluginImplCheckActionState =
            (TppGmPlayerImplClimbActionPluginImplCheckActionStateFunc *)AddressOf("TppGmPlayerImplClimbActionPluginImplCheckActionState");
        FoxGeoPathResultGetNumPoint = (FoxGeoPathResultGetNumPointFunc *)AddressOf("FoxGeoPathResultGetNumPoint");
        TppUiMenuUiCommonDataManagerLoadLanguageBlock =
            (TppUiMenuUiCommonDataManagerLoadLanguageBlockFunc *)AddressOf("TppUiMenuUiCommonDataManagerLoadLanguageBlock");
        TppUiUtilityChangeLanguage = (TppUiUtilityChangeLanguageFunc *)AddressOf("TppUiUtilityChangeLanguage");
        TppGmPlayerImplEquipControllerImplInitialize =
            (TppGmPlayerImplEquipControllerImplInitializeFunc *)AddressOf("TppGmPlayerImplEquipControllerImplInitialize");
        TppGmPlayerImplEquipControllerImplGetEquipIdSlot =
            (TppGmPlayerImplEquipControllerImplGetEquipIdSlotFunc *)AddressOf("TppGmPlayerImplEquipControllerImplGetEquipIdSlot");
        TppGmBossquietImplActionControllerImplSetCommand =
            (TppGmBossquietImplActionControllerImplSetCommandFunc *)AddressOf("TppGmBossquietImplActionControllerImplSetCommand");
        TppGmBossquietImplActionControllerImplSetExtraActionCommand =
            (TppGmBossquietImplActionControllerImplSetExtraActionCommandFunc *)AddressOf("TppGmBossquietImplActionControllerImplSetExtraActionCommand");
        TppGmLiquidImplActionControllerImplSetActionTaskCompleted =
            (TppGmLiquidImplActionControllerImplSetActionTaskCompletedFunc *)AddressOf("TppGmLiquidImplActionControllerImplSetActionTaskCompleted");
        TppGmBossquietImplActionControllerImplSetActionTask =
            (TppGmBossquietImplActionControllerImplSetActionTaskFunc *)AddressOf("TppGmBossquietImplActionControllerImplSetActionTask");
        TppGmBossquietImplActionControllerImplSetNextActionTask =
            (TppGmBossquietImplActionControllerImplSetNextActionTaskFunc *)AddressOf("TppGmBossquietImplActionControllerImplSetNextActionTask");
        TppGmBossquietImplActionControllerImplInitialize =
            (TppGmBossquietImplActionControllerImplInitializeFunc *)AddressOf("TppGmBossquietImplActionControllerImplInitialize");
        TppGmSahelanImplActionControllerImplSetTaskParamFromActionCommand = (TppGmSahelanImplActionControllerImplSetTaskParamFromActionCommandFunc *)
            AddressOf("TppGmSahelanImplActionControllerImplSetTaskParamFromActionCommand");
        TppGmImplNavigationController2ImplGetNavigator =
            (TppGmImplNavigationController2ImplGetNavigatorFunc *)AddressOf("TppGmImplNavigationController2ImplGetNavigator");
        TppGmImplNavigationController2ImplIsCurrentRequestPrepared =
            (TppGmImplNavigationController2ImplIsCurrentRequestPreparedFunc *)AddressOf("TppGmImplNavigationController2ImplIsCurrentRequestPrepared");
        TppGmBossquietImplActionControllerImplCheckUseOutOfNavJump =
            (TppGmBossquietImplActionControllerImplCheckUseOutOfNavJumpFunc *)AddressOf("TppGmBossquietImplActionControllerImplCheckUseOutOfNavJump");
        TppGmImplNavigationController2ImplGetPath = (TppGmImplNavigationController2ImplGetPathFunc *)AddressOf("TppGmImplNavigationController2ImplGetPath");
        TppGmImplNavigationController2ImplGetResultState =
            (TppGmImplNavigationController2ImplGetResultStateFunc *)AddressOf("TppGmImplNavigationController2ImplGetResultState");
        TppGmBossquietImplSnipeManagerImplGetOptimalLandingPosition =
            (TppGmBossquietImplSnipeManagerImplGetOptimalLandingPositionFunc *)AddressOf("TppGmBossquietImplSnipeManagerImplGetOptimalLandingPosition");
        TppGmSahelanActionTaskoperatorAssign = (TppGmSahelanActionTaskoperatorAssignFunc *)AddressOf("TppGmSahelanActionTaskoperatorAssign");
        TppGmBossquietImplActionControllerImplUpdateActionTask =
            (TppGmBossquietImplActionControllerImplUpdateActionTaskFunc *)AddressOf("TppGmBossquietImplActionControllerImplUpdateActionTask");
        TppGmBossquietImplActionControllerImplUpdate =
            (TppGmBossquietImplActionControllerImplUpdateFunc *)AddressOf("TppGmBossquietImplActionControllerImplUpdate");
        TppGmBossquietImplBasicAction2ImplPolling = (TppGmBossquietImplBasicAction2ImplPollingFunc *)AddressOf("TppGmBossquietImplBasicAction2ImplPolling");
        TppGmBossquietImplActionControllerImplUpdateCommandForSingle =
            (TppGmBossquietImplActionControllerImplUpdateCommandForSingleFunc *)AddressOf("TppGmBossquietImplActionControllerImplUpdateCommandForSingle");
        TppGmBossquietImplBasicAction2ImplGetNextPositionInMoving =
            (TppGmBossquietImplBasicAction2ImplGetNextPositionInMovingFunc *)AddressOf("TppGmBossquietImplBasicAction2ImplGetNextPositionInMoving");
        TppGmBossquietImplCloseCombatAiImplStepMove =
            (TppGmBossquietImplCloseCombatAiImplStepMoveFunc *)AddressOf("TppGmBossquietImplCloseCombatAiImplStepMove");
        TppGmBossquietImplCombatAiImplRequestMoveAction =
            (TppGmBossquietImplCombatAiImplRequestMoveActionFunc *)AddressOf("TppGmBossquietImplCombatAiImplRequestMoveAction");
        TppGmBossquietImplRecoveryAiImplStepMove = (TppGmBossquietImplRecoveryAiImplStepMoveFunc *)AddressOf("TppGmBossquietImplRecoveryAiImplStepMove");
        TppGmBossquietImplSearchAiImplStepMove = (TppGmBossquietImplSearchAiImplStepMoveFunc *)AddressOf("TppGmBossquietImplSearchAiImplStepMove");
        TppGmBossquietImplSearchAiImplRequestMoveAction =
            (TppGmBossquietImplSearchAiImplRequestMoveActionFunc *)AddressOf("TppGmBossquietImplSearchAiImplRequestMoveAction");
        TppGmBossquietImplRecoveryAiImplRequestMoveAction =
            (TppGmBossquietImplRecoveryAiImplRequestMoveActionFunc *)AddressOf("TppGmBossquietImplRecoveryAiImplRequestMoveAction");
        TppGmBossquietImplCloseCombatAiImplRequestMoveAction =
            (TppGmBossquietImplCloseCombatAiImplRequestMoveActionFunc *)AddressOf("TppGmBossquietImplCloseCombatAiImplRequestMoveAction");
        TppGmBossquietImplAvoidAiImplSetJumpPoint = (TppGmBossquietImplAvoidAiImplSetJumpPointFunc *)AddressOf("TppGmBossquietImplAvoidAiImplSetJumpPoint");
        TppGmImplAiControllerImplGetCoreKnowledge = (TppGmImplAiControllerImplGetCoreKnowledgeFunc *)AddressOf("TppGmImplAiControllerImplGetCoreKnowledge");
        TppGmBossquietImplanonymous_namespaceLifeControllerImplUpdateLife = (TppGmBossquietImplanonymous_namespaceLifeControllerImplUpdateLifeFunc *)
            AddressOf("TppGmBossquietImplanonymous_namespaceLifeControllerImplUpdateLife");
    }
}