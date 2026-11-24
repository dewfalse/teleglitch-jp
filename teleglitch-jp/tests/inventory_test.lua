-- Run the shipped item definitions with the DDE catalog enabled.
function HasDDE() return true end
function MakeParent(child,parent) setmetatable(child,{__index=parent}) end
function AddReaction() end
function AddReactionRequirement() end
dofile("lua/relvad.lua")

local seen, byEnglish = {}, {}
local count = 0
for line in io.lines("teleglitch-jp/inventory_ja.tsv") do
    if line:sub(1,1) ~= "#" then
        local id, english, ja = line:match("^([^\t]+)\t([^\t]+)\t(.+)$")
        assert(id and itemtable[id] and not seen[id],line)
        local original = itemtable[id].longtext:gsub("%s+"," "):match("^%s*(.-)%s*$")
        assert(english == original,"HUD source mismatch: "..id)
        assert(not byEnglish[english] or byEnglish[english] == ja,"Conflicting translations: "..id)
        byEnglish[english], seen[id] = ja, true
        count = count + 1
    end
end
for id,item in pairs(itemtable) do
    if item.longtext and item.longtext:find("%a") then
        assert(seen[id],"Missing inventory description: "..id)
    end
end
print("PASS: selected inventory descriptions match "..count.." item definitions including DDE")

dofile("lua/monsters.lua")
local translated = {}
local monsters = 0
for line in io.lines("teleglitch-jp/monsters_ja.tsv") do
    if line:sub(1,1) ~= "#" then
        local id, english, ja = line:match("^([^\t]+)\t([^\t]+)\t(.+)$")
        assert(id and enemyhelptable[id] and not translated[id] and ja,line)
        local original = enemyhelptable[id].description:gsub("%s+"," "):match("^%s*(.-)%s*$")
        original = original:gsub("\150","–")
        assert(english == original,"Enemy info source mismatch: "..id)
        translated[id] = true
        monsters = monsters + 1
    end
end
for id in pairs(enemyhelptable) do assert(translated[id],"Missing enemy info: "..id) end
print("PASS: info descriptions match all "..monsters.." enemy definitions")
