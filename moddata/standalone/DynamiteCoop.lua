-- Shared co-op lifecycle. Native mutations run on the game thread.
local this = {}
local original, state = {}, {}

local function freeRoam()
	return vars.missionCode == 30010 or vars.missionCode == 30020
end

local function guestQuest()
	return freeRoam() and not Dynamite.IsHost() and TppGameStatus.IsSet("Mission", "S_IS_ONLINE")
end

local function logOnce(key, text)
	state.logged = state.logged or {}
	if not state.logged[key] then
		state.logged[key] = true
		Dynamite.Log(text)
	end
end

function this.InstallQuestHooks()
	if original.search then return end
	original.search = TppQuest.SearchQuestFromAllSpecifiedArea
	original.inside = TppQuest.IsInsideArea
	original.nextStep = TppQuest.SetNextQuestStep
	original.face = TppQuest.GetRandomFaceId
	TppQuest.SearchQuestFromAllSpecifiedArea = function(...)
		if not guestQuest() then return original.search(...) end
		local q = state.quest
		-- Keep the last host snapshot during packet loss. A timeout is not a request to unload.
		if q and q.name ~= "" then return q.name end
		return nil
	end
	TppQuest.IsInsideArea = function(kind, area, ...)
		if not guestQuest() then return original.inside(kind, area, ...) end
		local q = state.quest
		if not q or q.name == "" or not mvars.qst_questList then return false end
		local hostArea = TppQuest.GetQuestTable(q.name)
		if area ~= hostArea then return false end
		if kind == "loadArea" then return true end
		if kind == "activeArea" then return q.active end
		if kind == "invokeArea" then return q.invoked end
		return original.inside(kind, area, ...)
	end
	TppQuest.SetNextQuestStep = function(...)
		if not guestQuest() then return original.nextStep(...) end
	end
	TppQuest.GetRandomFaceId = function(name)
		local q = state.quest
		if guestQuest() and q and q.face < 65536 and (name == nil or name == q.name) then return q.face end
		return original.face(name)
	end
	-- The guest observes host progression. Do not replay rewards, staff transfers,
	-- story unlocks or save operations when applying a remote step.
	for _, name in ipairs({ "ClearWithSave", "Clear", "Failure", "Save", "ClearWithSaveMtbsDDQuest" }) do
		local saved = TppQuest[name]
		if saved then
			original[name] = saved
			TppQuest[name] = function(...)
				if not guestQuest() then return saved(...) end
			end
		end
	end
end

function this.BeginMission()
	if DynamiteBuddy then DynamiteBuddy.BeginMission() end
	state = { nextHello = 0, nextQuest = 0, nextPoll = 0, logged = {} }
	mvars.dyn_gameStarted = false
	mvars.dyn_mission13ReloadRequested = nil
	-- This transient flag must survive replacement of mission Lua modules/mvars.
	-- Never put it in gvars, which belongs to the player's save.
	if vars.missionCode ~= 10080 or not TppPackList.IsMissionPackLabel("afterPumpStopDemo") then
		_G.DynamiteMission13Reconnect = nil
	end
	if not Dynamite.SetMissionContext then return end
	Dynamite.SetMissionContext(vars.missionCode, false, TppMain.IsCoop(vars.missionCode))
	this.InstallQuestHooks()
end

function this.StartMission()
	if vars.missionCode == 10080 and TppMain.IsCoop(vars.missionCode)
		and TppGameStatus.IsSet("Mission", "S_IS_ONLINE") then
		-- The opening sequence is skipped on the post-explosion pack and on continue.
		TppCoder.SetWorldCenter(Vector3(-569.03827, -3.9259033, 312.8404))
		if Dynamite.IsHost() then TppNetworkUtil.SessionEnableAccept(true) end
		Dynamite.StartNearestEnemyThread()
		if _G.DynamiteMission13Reconnect then
			state.reconnect = { attempts = 0, nextAttempt = Time.GetRawElapsedTimeSinceStartUp() + 2 }
			_G.DynamiteMission13Reconnect = nil
		end
		Dynamite.Log("mission 13: co-op initialized after mission loading")
	end
	if not Dynamite.SetMissionContext then return end
	Dynamite.SetMissionContext(vars.missionCode, true, TppMain.IsCoop(vars.missionCode))
end

function this.RememberMissionReload()
	local scene = mvars.dyn_missionScene
	if not scene or not scene.dynamiteCoopScene or scene.dynamiteSessionClosed then return end
	if mvars.dyn_mission13ReloadRequested and not Dynamite.IsHost()
		and (Dynamite.PartnerInSession() or state.connected) then
		_G.DynamiteMission13Reconnect = true
	end
end

function this.PrepareMissionReload()
	local scene = mvars.dyn_missionScene
	if not scene or not scene.dynamiteCoopScene or scene.dynamiteSessionClosed then return end
	this.RememberMissionReload()
	Dynamite.Log("mission 13: stopping co-op before mission objects are unloaded")
	TppMain.DynamiteStopCoopSession(scene)
end

function this.EndMission()
	if DynamiteBuddy then DynamiteBuddy.EndMission() end
	mvars.dyn_gameStarted = false
	if Dynamite.SetMissionContext then Dynamite.SetMissionContext(vars.missionCode, false, false) end
	state.quest = nil
	state.reconnect = nil
end

local function updateMissionReconnect(t)
	local retry = state.reconnect
	if not retry then return end
	if vars.missionCode ~= 10080 or Dynamite.IsHost() or Dynamite.PartnerInSession() then
		state.reconnect = nil
		return
	end
	if t < retry.nextAttempt then return end
	if retry.attempts >= 3 then
		state.reconnect = nil
		Dynamite.Log("mission 13: automatic reconnect exhausted; manual connection remains available")
		return
	end
	-- A failed native join keeps the session-created flag set. Release it before retrying.
	TppNetworkUtil.SessionDisconnectPreparingMembers()
	TppNetworkUtil.CloseSession()
	Dynamite.ResetClientSessionState()
	Dynamite.SetMissionContext(vars.missionCode, true, true)
	Dynamite.CreateClientSession()
	retry.attempts = retry.attempts + 1
	retry.nextAttempt = t + 20
	Dynamite.Log("mission 13: reconnect attempt " .. retry.attempts .. "/3")
end

local function traceMission13(t)
	if vars.missionCode ~= 10080 or t < (state.nextTrace or 0) then return end
	state.nextTrace = t + 10
	local connected = Dynamite.PartnerInSession()
	local ready = Dynamite.CoopReady and Dynamite.CoopReady() or false
	Dynamite.Log("mission 13 sync: host=" .. tostring(Dynamite.IsHost()) .. " connected=" .. tostring(connected)
		.. " ready=" .. tostring(ready) .. " local=" .. tostring(vars.playerPosX) .. "," .. tostring(vars.playerPosY)
		.. "," .. tostring(vars.playerPosZ))
end

local function updateQuest(t)
	if not freeRoam() or not Dynamite.CoopReady() then return end
	if Dynamite.IsHost() then
		if t < state.nextQuest then return end
		state.nextQuest = t + 0.5
		local name = TppQuest.GetCurrentQuestName() or ""
		local steps = mvars.qst_questStepList
		local step = steps and steps[gvars.qst_currentQuestStepNumber] or ""
		local active = TppQuest.GetQuestBlockState() == ScriptBlock.SCRIPT_BLOCK_STATE_ACTIVE
		Dynamite.SendQuestState(name, step or "", name ~= "" and active,
			name ~= "" and TppQuest.IsInvoking(), name ~= "" and TppQuest.GetRandomFaceId(name) or 65536)
	else
		local name, step, active, invoked, face = Dynamite.PopQuestState()
		if name ~= nil and mvars.qst_questList then
			if name == "" or (TppDefine.QUEST_INDEX[name] and TppQuest.GetQuestTable(name)) then
				state.quest = { name = name, step = step, active = active, invoked = invoked, face = face }
			else
				logOnce("quest" .. name, "Side Op snapshot ignored: quest not available in this location: " .. name)
			end
		end
		local q = state.quest
		if not q then return end
		if t >= state.nextPoll then
			state.nextPoll = t + 0.25
			-- Reevaluate the stock asynchronous loader even when neither player moves.
			-- Never replace/free a PROCESSING block or call a step on an unallocated script.
			if TppQuest.GetQuestBlockState() ~= ScriptBlock.SCRIPT_BLOCK_STATE_PROCESSING then
				TppQuest.OnUpdateSmallBlockIndex(Tpp.GetCurrentStageSmallBlockIndex())
			end
		end
		if q.name ~= "" and q.name == TppQuest.GetCurrentQuestName() and q.active and q.invoked
			and TppQuest.GetQuestBlockState() == ScriptBlock.SCRIPT_BLOCK_STATE_ACTIVE
			and mvars.qst_allocated and not mvars.qst_lastInactiveToActive
			and mvars.qst_questStepList and mvars.qst_questStepTable
			and mvars.qst_questStepTable[q.step] and mvars.qst_questStepList[q.step]
			and mvars.qst_questStepList[gvars.qst_currentQuestStepNumber] ~= q.step then
			original.nextStep(q.step)
		end
	end
end

function this.Update()
	if DynamiteBuddy then DynamiteBuddy.RestoreTick() end
	if not Dynamite.ProcessIncoming then return end
	if not (mvars.dyn_gameStarted and TppMain.IsCoop(vars.missionCode)
		and TppGameStatus.IsSet("Mission", "S_IS_ONLINE")) then return end
	Dynamite.ProcessIncoming()
	if freeRoam() and not state.accepting and Dynamite.IsHost() then
		TppNetworkUtil.SessionEnableAccept(true)
		Dynamite.StartNearestEnemyThread()
		state.accepting = true
	end
	local t = Time.GetRawElapsedTimeSinceStartUp()
	updateMissionReconnect(t)
	traceMission13(t)
	if t >= state.nextHello then
		state.nextHello = t + 0.5
		Dynamite.SendCoopHello()
	end
	if not Dynamite.PartnerInSession() then
		if state.connected then
			if DynamiteBuddy then DynamiteBuddy.Disconnect() end
			state.connected = false
			state.quest = nil
			Dynamite.SetMissionContext(vars.missionCode, false, true)
			Dynamite.SetMissionContext(vars.missionCode, true, true)
		end
		return
	end
	state.connected = true
	if DynamiteBuddy then DynamiteBuddy.Update(t) end
	updateQuest(t)
end

return this
