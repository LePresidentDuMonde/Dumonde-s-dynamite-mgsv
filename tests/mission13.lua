-- Production mission/loader/co-op code; only engine services are simulated.
local function noop() end
local function enum(t) for i, name in ipairs(t) do t[name]=i end; return t end
local function service() return setmetatable({}, {__index=function() return noop end}) end
local host, partner, online, now, label = false, true, true, 0, "default"
local calls, timers, broken, seqs = {}, {}, {}, nil
local function record(name) calls[#calls+1]=name end
local function count(name) local n=0; for _,v in ipairs(calls) do if v==name then n=n+1 end end; return n end
Vector3=function(x,y,z) return {x=x,y=y,z=z} end
Fox={StrCode32=function(v) return v end, Log=noop}
Tpp={Enum=enum, StrCode32Table=function(v) return v end, IsTypeFunc=function(v) return type(v)=="function" end,
	IsTypeTable=function(v) return type(v)=="table" end, IsTypeString=function(v) return type(v)=="string" end,
	IsHorse=function() return false end}
TppDefine={WEATHER={CLOUDY=1}, TRAP_TYPE={NORMAL=1}, TRAP_STATE={DISABLE=1}, INIT_HELI_ROUTE="init"}
TppDefine.MISSION_CLEAR_TYPE={ON_FOOT=1,RIDE_ON_HELICOPTER=2,RIDE_ON_VEHILCE=3,RIDE_ON_FULTON_CONTAINER=4}
EntryBuddyType={VEHICLE=1,BUDDY=2}
TppMath={DegreeToRadian=function(v) return v end}
PlayerPad={STOCK=1,STICK_R=2,STICK_L=4}
PlayerCamera={Around=1}
Player={StartTargetConstrainCamera=noop,SetPadMask=function() record("mask") end,ResetPadMask=noop}
StageBlock={ACTIVE=1}
GameObject={NULL_ID=65535,GetGameObjectId=function(v) return v end,SendCommand=noop}
GkEventTimerManager={Start=function(name) timers[name]=true end,Stop=function(name) timers[name]=nil end,StopAll=noop}
Gimmick=service()
TppGimmick={GetGimmickID=function(_,name) return name end,Hide=noop,IsBroken=function(v) return broken[v.gimmickId] end}
mafr_gimmick={gimmickIdentifierParamTable={}}
for _,name in ipairs({"flowStation_tank005","flowStation_tank003_00","flowStation_tank003_01","flowStation_tank003_02","flowStation_tank005_vrtn006"}) do
	mafr_gimmick.gimmickIdentifierParamTable[name]={type=1,locatorName=name,dataSetName="flowStation"}
end
TppDataUtility=service()
s10080_enemy=service()
s10080_enemy.ENEMY_NAME={TEACHER1="teacher1",TEACHER2="teacher2"}
s10080_enemy.CHILD_NAME={CHILD1="child1",CHILD2="child2",CHILD3="child3",CHILD4="child4"}
TppCoder={SetWorldCenter=function(v)
	assert(v.x==-569.03827 and v.y==-3.9259033 and v.z==312.8404,"wrong post-reload network origin")
	record("origin")
end}
TppGameStatus={IsSet=function() return online end,Reset=function() online=false end}
TppNetworkUtil={SessionEnableAccept=function(v) record(v and "accept" or "reject") end,
	SessionDisconnectPreparingMembers=function() record("disconnect") end,
	CloseSession=function() record("close"); partner=false end}
Time={GetRawElapsedTimeSinceStartUp=function() return now end}
TppPackList={IsMissionPackLabel=function(v) return label==v end,SetMissionPackLabelName=function(v) label=v end}
TppScriptVars={}
ScriptBlock={}
Mission={}
PlayRecord={}
DemoDaemon={}
bit={}
TppQuest={}
TppUI={FADE_SPEED={FADE_NORMALSPEED=1},FadeOut=function(_,sender) assert(sender=="ReloadFadeOutFinish"); record("fade") end}
TppSequence={RegisterSequences=noop,RegisterSequenceTable=function(t) seqs=t end,
	ReserveNextSequence=function(name) record(name) end,SetNextSequence=function(name) record(name) end,
	IsLandContinue=function() return false end}
TppSave={VarSave=noop,CheckAndSavePersonalData=noop}
TppPlayer={ForceSetAllInitialWeapon=noop}
SubtitlesCommand=service(); TppRadio=service(); TppMusicManager=service(); TppRadioCommand=service()
TppBuddy2BlockController={Unload=function() assert(count("close")==1,"unloaded mission objects with live session"); record("unload") end}
TppSound=service(); TppSoundDaemon=service(); TppOutOfMissionRangeEffect=service(); TppTerminal=service()
Dynamite={IsHost=function() return host end, PartnerInSession=function() return partner end,
	SetMissionContext=noop,ProcessIncoming=noop,SendCoopHello=noop,Log=noop,
	IgnoreMarkerRequests=function() record("ignore") end,
	StopNearestEnemyThread=function() record("stop") end,StartNearestEnemyThread=function() record("start") end,
	ResetClientSessionState=function() record("reset") end,CreateClientSession=function() record("join") end}
vars={missionCode=10080}; mvars={}; gvars={}
TppMain=assert(loadfile("moddata/standalone/TppMain.lua"))()
TppMission=assert(loadfile("moddata/standalone/TppMission.lua"))()
DynamiteCoop=assert(loadfile("moddata/standalone/DynamiteCoop.lua"))()
TppMission.UpdateCheckPoint=function(v) assert(v.checkPoint); record("checkpoint") end
TppMission.RequestLoad=function(code,_,options)
	assert(code==10080 and options.force and label=="afterPumpStopDemo")
	record("load")
end
local mission=assert(loadfile("moddata/s10080_area_fpkd/Assets/tpp/level/mission2/story/s10080/s10080_sequence.lua"))()
mission.OnLoad()
svars={}; for k,v in pairs(mission.saveVarsList) do svars[k]=v end
local function begin()
	mvars={dyn_missionScene={mission={},dynamiteCoopScene=true},dyn_gameStarted=false}
	DynamiteCoop.BeginMission()
end
local function start()
	mvars.seq_demoSequneceList={}; mvars.seq_missionStartSequence="post-explosion"
	TppClock={Start=noop}; PlayRecord.RegistPlayRecord=noop
	TppQuest.InitializeQuestActiveStatus=noop
	TppMain.EnableGameStatus=noop
	TppMain.OnMissionGameStart({})
end
begin()
svars.isLoadedFlowStation=true
svars.isInFlowStation=true
svars.isFultoned_child1=mission.FLOWSTATIONAREA_STATUS.TANK_UNDER1
Gimmick.BreakGimmick=function(_,name)
	broken[name]=true
	seqs.Seq_Game_StopFlowStation.FuncBreakedTank(1,name,0)
end
-- A real objective tank must run the production chain, not an unrelated barrel.
seqs.Seq_Game_StopFlowStation.FuncBreakedTank(1,"unrelated_barrel",0)
assert(not timers.explosionDeray and not svars.isBreakedTank)
Gimmick.BreakGimmick(1,"flowStation_tank005")
for _=1,8 do
	for i=1,4 do
		local name="breakTankTimer"..i
		if timers[name] then timers[name]=nil; seqs.Seq_Game_StopFlowStation["FuncBreakTank"..i]() end
	end
end
assert(timers.explosionDeray and svars.isBreakedTank,"explosion chain never reached reload")
local messages=seqs.Seq_Game_StopFlowStation:Messages()
for _,v in ipairs(messages.Timer) do if v.sender=="explosionDeray" then v.func(); v.func() end end
assert(count("fade")==1 and count("checkpoint")==1,"duplicate reload requested")
assert(count("Seq_Demo_Reinforcement")==1,"reinforcement cinematic not reserved")
-- The host may finish its fade sooner and disconnect the guest before teardown.
assert(DynamiteMission13Reconnect,"guest was not remembered before the fade")
partner=false
TppMission.ExecuteReload()
assert(count("stop")==1 and count("ignore")==1 and count("close")==1 and count("load")==1)
assert(DynamiteMission13Reconnect,"connected guest not remembered")
TppMain.OnTerminate(mvars.dyn_missionScene.mission)
assert(count("close")==1,"session closed twice")
-- Simulate replacement of the co-op Lua module and mvars by the engine loader.
DynamiteCoop=assert(loadfile("moddata/standalone/DynamiteCoop.lua"))()
begin(); online=true; now=10
start()
assert(count("origin")==1 and count("start")==1 and count("accept")==0)
assert(not DynamiteMission13Reconnect)
DynamiteCoop.Update(); assert(count("join")==0,"joined before post-load delay")
now=12; DynamiteCoop.Update(); assert(count("join")==1)
now=31; DynamiteCoop.Update(); assert(count("join")==1,"unbounded retry")
now=32; DynamiteCoop.Update(); assert(count("join")==2)
now=52; DynamiteCoop.Update(); assert(count("join")==3)
now=100; DynamiteCoop.Update(); assert(count("join")==3,"more than three retries")
-- Opening/continued/host loads share the same world origin and reopen admission.
host=true; begin(); online=true; start()
assert(count("origin")==2 and count("accept")==1 and count("start")==2)
-- Offline guests and ordinary mission transitions never opt into auto-join.
host=false; partner=false; begin(); online=true
mvars.dyn_mission13ReloadRequested=true
DynamiteCoop.PrepareMissionReload()
assert(not DynamiteMission13Reconnect,"solo load enabled automatic join")
label="default"; begin(); online=true; start()
now=200; DynamiteCoop.Update(); assert(count("join")==3)
-- A successful native join cancels all remaining scheduled attempts.
DynamiteMission13Reconnect=true; label="afterPumpStopDemo"; begin(); online=true; start()
now=202; DynamiteCoop.Update(); assert(count("join")==4)
partner=true; now=203; DynamiteCoop.Update()
partner=false; now=250; DynamiteCoop.Update(); assert(count("join")==4)
-- Keep the stock reinforcement callback and both objective orders.
s10080_radio=service()
TppMission.UpdateObjective=function(v) if v.objectives[1]=="clear_missionTask_02" then record("tank-objective") end end
svars.isStopedPump=false; mission.FuncTankDemo()()
assert(count("Seq_Game_StopFlowStation")==1,"tank-first path skipped the pump objective")
svars.isStopedPump=true; mission.FuncTankDemo()()
assert(count("Seq_Game_Escape")==1 and count("tank-objective")==2,"pump-first path failed to enable escape")
print("PASS: mission 13 explosion chain, single reload, teardown before unload, network origin, host admission and bounded guest reconnect")
