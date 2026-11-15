-- Execute the production personal-buddy lifecycle. Equipment access/reloads are forbidden.
local updates,connected,resets={},true,0
local poses,available={},true
local selection={buddyType=2,buddyDogEquipType=3,buddyHorseType=1}
vars=setmetatable({}, {__index=function(_,key) return selection[key] end,
 __newindex=function() error("buddy loadout was overwritten") end})
TppBuddy2BlockController=setmetatable({}, {__index=function() error("buddy block was forced/reloaded") end})
Dynamite={CoopReady=function() return connected end,
 ResetPersonalBuddyScans=function() resets=resets+1 end,
 UpdateDogReplicas=function(active,selected) poses[#poses+1]={active,selected}; return available end,
 UpdatePersonalBuddy=function(active) updates[#updates+1]=active end}
local buddy=dofile("moddata/standalone/DynamiteBuddy.lua")
buddy.BeginMission()
assert(#updates==1 and updates[1]==false)
buddy.Update(1)
buddy.Update(1.01)
assert(#updates==2 and updates[2]==true,"personal scans did not tick or were unbounded per frame")
assert(#poses==1 and poses[1][1] and poses[1][2],"Dog skeletal updates did not retain local selection")
buddy.Update(1.06)
assert(#poses==2 and #updates==2,"pose and scan update rates were not independent")
connected=false
buddy.Update(2)
assert(updates[3]==false,"disconnect retained shared scans")
buddy.Disconnect()
assert(not poses[#poses][1],"disconnected Dog replica kept rendering")
assert(resets==1,"same-world disconnect cleared the local Dog's detections")
buddy.BeginMission()
assert(resets==2,"mission reload retained old scan IDs")
connected=true
buddy.Update(0)
assert(updates[#updates]==true,"new mission clock did not reset")
buddy.RestoreTick()
buddy.EndMission()
assert(updates[#updates]==false)
-- Each instance can use either kind without touching personal equipment.
for _,kind in ipairs({1,2}) do
 selection.buddyType=kind
 buddy.BeginMission(); buddy.Update(5); buddy.EndMission()
 assert(vars.buddyType==kind and vars.buddyDogEquipType==3 and vars.buddyHorseType==1)
end
available=false
local hidden=0
GameObject={GetGameObjectIdByIndex=function(kind,index) assert(kind=="TppBuddyDog2" and index==1);return 9729 end,
 SendCommand=function(id,command) assert(id==9729 and command.id=="SetEnabled" and not command.enabled);hidden=hidden+1 end}
buddy.BeginMission();buddy.Update(0);buddy.Update(1)
assert(hidden==1,"unavailable native adapter did not safely disable only the extra Dog")
print("PASS: personal buddy lifecycle, independent loadouts, bounded scan updates, disconnect, mission reload")
print("PASS: Dog pose scheduling, native-adapter fallback, independent scan updates and replica cleanup")
