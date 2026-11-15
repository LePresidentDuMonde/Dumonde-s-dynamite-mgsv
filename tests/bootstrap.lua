-- Exercise the real OnAllocate module traversal, including the title scene.
local function noop() end
local function service() return setmetatable({}, {__index=function() return noop end}) end
Tpp={IsTypeFunc=function(v) return type(v)=="function" end,IsTypeTable=function(v) return type(v)=="table" end}
TppScriptVars={IsSavingOrLoading=function() return false end}
ScriptBlock={UpdateScriptsInScriptBlocks=noop}
Mission={GetCurrentMessageResendCount=noop,Start=noop}
TppMission={IsFOBMission=function() return false end,DisableInGameFlag=noop,
	WaitFinishMissionEndPresentation=noop,OnAllocate=noop}
for _,name in ipairs({"TppWeather","TppClock","TppSave","TppException","TppTrap","TppCheckPoint",
	"TppDemo","TppScriptBlock","TppSound","TppPlayer","TppTerminal","TppEnemy","TppRadio",
	"TppGimmick","TppMarker","TppRevenge","TppQuest","TppAnimal","TppLocation"}) do _G[name]=service() end
TppUI={FADE_SPEED={FADE_MOMENT=0},FadeOut=noop,OnAllocate=noop}
StageBlock={ClearLargeBlockNameForMessage=noop,ClearSmallBlockIndexForMessage=noop}
TppPackList={IsMissionPackLabel=function() return false end}
local online, closes, terminated=false,0,0
local originPrepared=false
Vector3=function(x,y,z) return {x,y,z} end
TppCoder={SetWorldCenter=function() originPrepared=true end}
Mission.Start=function()
	assert(not originPrepared,"network origin changed during scene allocation")
end
TppGameStatus={Set=function() online=true end,Reset=function() online=false end,IsSet=function() return online end}
TppNetworkUtil={SetTimeOut=noop,SessionEnableAccept=noop,SessionDisconnectPreparingMembers=noop,
	CloseSession=function() closes=closes+1 end}
Dynamite={StopNearestEnemyThread=noop,IgnoreMarkerRequests=noop,CreateHostSession=function()
	assert(not originPrepared,"network origin changed while creating native session")
end,Log=noop,
	IsHost=function() return true end,ResetClientSessionState=noop,SetMissionContext=noop}
TppMain=assert(loadfile(arg[1] or "moddata/standalone/TppMain.lua"))()
DynamiteCoop=assert(loadfile("moddata/standalone/DynamiteCoop.lua"))()
TppMain.DisableGameStatus=noop
TppMain.EnablePause=noop
local reached={}
for _,code in ipairs({5,10080,30010}) do
	vars={missionCode=code}; mvars={}; gvars={}
	originPrepared=false
	local modules={}
	modules.sequence={OnLoad=function()
		-- No booleans or metadata tables may masquerade as mission modules.
		for name,value in pairs(modules) do
			assert(name=="sequence" and type(value)=="table","co-op metadata leaked into module list")
		end
		reached[code]=true
		error("bootstrap module traversal reached",0)
	end,OnTerminate=function() terminated=terminated+1 end}
	local ok,err=pcall(TppMain.OnAllocate,modules)
	assert(not ok and err=="bootstrap module traversal reached",tostring(code)..": "..tostring(err))
	assert(reached[code],"loader failed before module initialization")
	assert(mvars.dyn_missionScene.mission==modules,"scene bookkeeping lost its module identity")
	TppMain.OnTerminate(modules)
	TppMain.OnTerminate(modules)
	assert(closes==(code==5 and 0 or code==10080 and 1 or 2),"wrong session cleanup count")
end
assert(terminated==6)
print("PASS: production OnAllocate traverses title, story and free-roam modules without metadata contamination")
