-- Exercise the production allocator with the real mission 13 budget override.
local function noop() end
Tpp={IsTypeFunc=function(v) return type(v)=="function" end,IsTypeTable=function(v) return type(v)=="table" end,
 IsTypeString=function(v) return type(v)=="string" end,Enum=function(v) return v end}
Fox={StrCode32=function(v) return v end}
GkEventTimerManager={Start=noop,Stop=noop}
GameObject={NULL_ID=65535}
TppGameObject={}
Vehicle={type={}}
for i,name in ipairs({"EASTERN_LIGHT_VEHICLE","EASTERN_TRACKED_TANK","EASTERN_TRUCK","EASTERN_WHEELED_ARMORED_VEHICLE",
 "WESTERN_LIGHT_VEHICLE","WESTERN_TRACKED_TANK","WESTERN_TRUCK","WESTERN_WHEELED_ARMORED_VEHICLE"}) do Vehicle.type[name]=i end
TppDefine={DEFAULT_EQUIP_MISSION_BLOCK_GROUP_SIZE=1777721,PICKABLE_MAX=16,PLACED_MAX=8}
TppDefine.WEATHER={SUNNY=1,CLOUDY=2,RAINY=3,FOGGY=4,SANDSTORM=5}
local coop=true
TppMain={IsCoop=function() return coop end}
Dynamite={Log=noop}
local allocation
TppEquip={CreateEquipMissionBlockGroup=function(v) allocation=v.size end}
vars={missionCode=10080}; mvars={}
local player=assert(loadfile("moddata/standalone/TppPlayer.lua"))()
local function allocate(code,budget,isCoop)
 vars.missionCode=code; coop=isCoop; allocation=nil
 player.OnAllocate({sequence={EQUIP_MISSION_BLOCK_GROUP_SIZE=budget}})
 player.SetEquipMissionBlockGroupSize()
 return allocation
end
assert(allocate(10080,1340000,true)==1777721,"mission override bypassed co-op render budget")
assert(allocate(10080,2000000,true)==2100000,"larger mission budget was reduced")
assert(allocate(10080,1340000,false)==1440000,"offline mission budget changed")
assert(allocate(10070,1340000,true)==1440000,"another mission budget changed")
assert(allocate(10080,nil,true)==1777721,"default budget changed")
print("PASS: production player allocator applies mission 13 co-op minimum, preserves larger and other mission budgets")
