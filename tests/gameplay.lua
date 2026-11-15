-- Executes the production integration against the extracted stock TppQuest implementation.
-- Only engine services (streaming, save I/O, transport and time) are replaced.
local analysis = assert(arg[1], "pass the local mgsv-analysis directory")
local function noop() end
local clock, host, connected, ready = 0, false, true, true
local incoming, sent, mutations, entered, loads = nil, nil, {}, {}, 0
local block = 0
local function enum(items)
	for index, name in ipairs(items) do items[name] = index end
	return items
end
Vector3 = function(x, y, z) return { GetX=function() return x end, GetY=function() return y end, GetZ=function() return z end } end
Fox = { StrCode32 = function(value) return value end }
Tpp = { Enum=enum, StrCode32Table=function(value) return value end, IsTypeFunc=function(value) return type(value)=="function" end,
	IsTypeTable=function(value) return type(value)=="table" end, IsTypeString=function(value) return type(value)=="string" end,
	GetCurrentStageSmallBlockIndex=function() return 12, 34 end, MakeMessageExecTable=function(value) return value end }
TppDefine = { Enum=enum, LOCATION_ID={AFGH=10,MAFR=20}, QUEST_INDEX={test_quest=1,other_quest=2}, QUEST_RANDOM_FACE_INDEX={test_quest=1} }
TppDefine.LOCATION_ID.MTBS=50
TppDefine.CLUSTER_DEFINE=enum({"Medical","Develop","Command","Support","BaseDev","Spy","Combat"})
TppDefine.PLNT_DEFINE=enum({"Special","Common1"})
TppDefine.STORY_SEQUENCE={CLEARD_FLAG_MISSIONS_AFTER_FIND_THE_SECRET_WEAPON=1}
TppMotherBaseManagementConst=setmetatable({}, {__index=function(_, key) return key end})
TppStory={}
TppBuddyService={}
BuddyCommonFlag={BUDDY_QUIET_LOST=1}
GameObject = { NULL_ID=65535, GetGameObjectId=noop, GetTypeIndex=noop }
ScriptBlock = { SCRIPT_BLOCK_STATE_EMPTY=0, SCRIPT_BLOCK_STATE_PROCESSING=1, SCRIPT_BLOCK_STATE_INACTIVE=2,
	SCRIPT_BLOCK_STATE_ACTIVE=3, SCRIPT_BLOCK_ID_INVALID=-1, GetScriptBlockId=function() return 1 end,
	GetScriptBlockState=function() return block end, UpdateScriptsInScriptBlocks=noop }
TppScriptBlock = {
	Load=function(_, name) loads=loads+1; block=1; return true end,
	Unload=function() block=0; TppQuest.QuestBlockOnTerminate() end,
	ActivateScriptBlockState=function() block=3 end, DeactivateScriptBlockState=function() block=2 end,
	FinalizeScriptBlockState=noop }
TppLocation = { IsMotherBase=function() return false end }
TppScriptVars = { IsSavingOrLoading=function() return false end }
Mission = { GetCurrentMessageResendCount=function() return 0 end }
Time = { GetRawElapsedTimeSinceStartUp=function() return clock end }
local online = true
TppGameStatus = { IsSet=function() return online end, Reset=function() online=false end }
TppNetworkUtil = { SessionDisconnectPreparingMembers=function() mutations.disconnect=(mutations.disconnect or 0)+1 end,
	CloseSession=function() mutations.close=(mutations.close or 0)+1 end,
	SessionEnableAccept=function(value) mutations.accept=value end }
TppSave = { VarSaveQuest=function() error("guest attempted to save quest") end, SaveGameData=function() error("guest attempted to save game") end }
TppPackList = { GetLocationNameFormMissionCode=function() return "AFGH" end }
vars = { missionCode=30010 }
gvars = { qst_currentQuestStepNumber=0, qst_randomFaceId={[1]=999} }
mvars = { dyn_gameStarted=true }
Dynamite = {
	IsHost=function() return host end, PartnerInSession=function() return connected end, CoopReady=function() return ready end,
	Log=noop, SetMissionContext=function(...) mutations.context={...} end, ProcessIncoming=noop, SendCoopHello=noop,
	SendQuestState=function(...) sent={...} end, PopQuestState=function() if incoming then local value=incoming; incoming=nil; return unpack(value) end end,
	StopNearestEnemyThread=noop, StartNearestEnemyThread=noop, ResetClientSessionState=function() mutations.reset=(mutations.reset or 0)+1 end }
Dynamite.IgnoreMarkerRequests=noop
TppQuest = assert(loadfile(analysis .. "/archives/extracted/master_0_00_dat/direct/Assets/tpp/script/lib/TppQuest.lua"))()
TppMain = assert(loadfile("moddata/standalone/TppMain.lua"))()
DynamiteCoop = assert(loadfile("moddata/standalone/DynamiteCoop.lua"))()
local area = { locationId=10, areaName="test", infoList={{name="test_quest",invokeStepName="Main"},{name="other_quest",invokeStepName="Main"}} }
mvars.qst_questList={area}
TppQuest.SetDefaultQuestBlock()
DynamiteCoop.BeginMission()
DynamiteCoop.StartMission()
mvars.dyn_gameStarted=true
assert(TppMain.IsCoop(30010) and TppMain.IsCoop(30020) and not TppMain.IsCoop(30050))
local savedSearch=TppQuest.SearchQuestFromAllSpecifiedArea
DynamiteCoop.InstallQuestHooks()
assert(savedSearch==TppQuest.SearchQuestFromAllSpecifiedArea, "hooks installed twice")
assert(TppQuest.SearchQuestFromAllSpecifiedArea("loadArea",12,34)==nil, "guest selected a quest before the host snapshot")
incoming={"test_quest","Main",true,true,123}
DynamiteCoop.Update()
assert(loads==1 and block==1 and TppQuest.GetCurrentQuestName()=="test_quest", "snapshot did not start stock asynchronous load")
assert(TppQuest.GetRandomFaceId()==123 and gvars.qst_randomFaceId[1]==999, "remote face overwrote guest save data")
clock=0.3
DynamiteCoop.Update()
assert(loads==1 and block==1, "PROCESSING block reloaded")
TppQuest.RegisterQuestStepList({"Start","Main"})
TppQuest.RegisterQuestStepTable({ Start={OnEnter=function() entered.Start=(entered.Start or 0)+1 end},
	Main={OnEnter=function() entered.Main=(entered.Main or 0)+1 end} })
block=2
clock=0.6
DynamiteCoop.Update()
TppQuest.QuestBlockOnUpdate()
TppQuest.QuestBlockOnUpdate()
TppQuest.QuestBlockOnUpdate()
DynamiteCoop.Update()
assert(block==3 and gvars.qst_currentQuestStepNumber==2 and entered.Main==1, "host step applied before allocation or not applied")
incoming={"test_quest","Main",true,true,123}
clock=1
DynamiteCoop.Update()
assert(entered.Main==1, "heartbeat replayed OnEnter")
clock=30
DynamiteCoop.Update()
assert(TppQuest.SearchQuestFromAllSpecifiedArea("loadArea",12,34)=="test_quest" and block==3, "packet loss unloaded quest")
TppQuest.ClearWithSave(1)
TppQuest.Clear()
TppQuest.Failure()
TppQuest.Save()
TppQuest.SetNextQuestStep("Start")
assert(gvars.qst_currentQuestStepNumber==2 and not entered.Start, "guest advanced quest locally")
incoming={"invalid_quest","Main",true,true,123}
DynamiteCoop.Update()
assert(TppQuest.GetCurrentQuestName()=="test_quest", "unknown quest changed live block")
incoming={"","",false,false,65536}
clock=31
DynamiteCoop.Update()
TppQuest.QuestBlockOnUpdate()
TppQuest.QuestBlockOnUpdate()
clock=31.5
DynamiteCoop.Update()
assert(block==0, "host retirement did not unload guest quest through stock lifecycle")
host=true
clock=32
DynamiteCoop.Update()
assert(sent and sent[1]=="", "host idle snapshot missing")
assert(mutations.accept==true, "free-roam host never opened the native session")
-- The actual TppMain update must drain networking before the entity/mission update.
local order={}
DynamiteCoop.Update=function() order[#order+1]="protocol" end
TppEnemy = { DynamiteOnUpdate=function() order[#order+1]="entities" end }
TppMain.OnUpdate({})
assert(order[1]=="protocol" and order[2]=="entities", "production update not connected")
local scene={sequence={OnTerminate=noop}}
mvars.dyn_missionScene={mission=scene,dynamiteCoopScene=false}
online=true
TppMain.OnTerminate(scene)
assert(not mutations.close, "helicopter lobby teardown closed prepared session")
scene={sequence={OnTerminate=noop}}
mvars.dyn_missionScene={mission=scene,dynamiteCoopScene=true}
TppMain.OnTerminate(scene)
TppMain.OnTerminate(scene)
assert(mutations.close==1 and mutations.reset==1, "gameplay teardown not idempotent or parameter shadowed TppMain")
-- Old DLL: unsupported extra APIs cannot interrupt story missions or enable free roam.
Dynamite.SetMissionContext=nil
assert(not TppMain.IsCoop(30010))
DynamiteCoop.BeginMission()
DynamiteCoop.StartMission()
DynamiteCoop.EndMission()
print("PASS: production Lua + extracted TppQuest: async loading, allocation, idempotence, packet loss, guest saves, faces, retirement, update wiring, lobby and teardown")
