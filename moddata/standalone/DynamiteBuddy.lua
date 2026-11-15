-- Each machine keeps its own stock companion and equipment. Only detections are exchanged.
local this = {}
local nextUpdate = 0
local nextPose = 0
local fallbackHidden = false

function this.BeginMission()
	nextUpdate = 0
	nextPose = 0
	fallbackHidden = false
	if Dynamite.ResetPersonalBuddyScans then Dynamite.ResetPersonalBuddyScans() end
	if Dynamite.UpdatePersonalBuddy then Dynamite.UpdatePersonalBuddy(false) end
end

function this.EndMission()
	nextUpdate = 0
	nextPose = 0
	if Dynamite.UpdateDogReplicas then Dynamite.UpdateDogReplicas(false, false) end
	if Dynamite.UpdatePersonalBuddy then Dynamite.UpdatePersonalBuddy(false) end
	if Dynamite.ResetPersonalBuddyScans then Dynamite.ResetPersonalBuddyScans() end
end

function this.Disconnect()
	nextUpdate = 0
	nextPose = 0
	if Dynamite.UpdateDogReplicas then Dynamite.UpdateDogReplicas(false, false) end
	-- The world and local Dog are still loaded; retain its detections for a reconnect.
	if Dynamite.UpdatePersonalBuddy then Dynamite.UpdatePersonalBuddy(false) end
end

-- Kept for the co-op lifecycle; there is no remote equipment to restore or reload.
function this.RestoreTick() end

function this.Update(t)
	if not Dynamite.UpdatePersonalBuddy then return end
	local active = Dynamite.CoopReady()
	if t >= nextPose and Dynamite.UpdateDogReplicas then
		nextPose = t + .05
		local available = Dynamite.UpdateDogReplicas(active, vars.buddyType == 2)
		if not available and not fallbackHidden and GameObject and GameObject.GetGameObjectIdByIndex then
			local id = GameObject.GetGameObjectIdByIndex("TppBuddyDog2", 1)
			if id == 9729 then
				fallbackHidden = pcall(GameObject.SendCommand, id, { id = "SetEnabled", enabled = false })
			end
		end
	end
	if t >= nextUpdate then
		nextUpdate = t + .1
		Dynamite.UpdatePersonalBuddy(active)
	end
end

return this
