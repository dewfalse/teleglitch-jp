local captured, smallCaptured, calls = nil, nil, 0
ShowMessage = function(text) captured=text calls=calls+1 end
ShowSmallMessage = function(text) smallCaptured=text end
levelname="tutorial"
dofile("teleglitch-jp/tutorial.lua")
local records = TeleglitchJP.tutorial_records
local count, covered = 0, {}
local function normalize(text) return text:gsub("%s+", " "):match("^%s*(.-)%s*$") end
local function width(text)
    local pixels=0
    for ch in text:gmatch("[%z\1-\127\194-\244][\128-\191]*") do
        pixels=pixels+(#ch==1 and 6 or 12)
    end
    return pixels
end
for id,record in pairs(records) do
    count=count+1
    assert(#record.rows<=7, id..": dialog too tall")
    for _,row in ipairs(record.rows) do
        local formatted=string.format(row,25,25)
        assert(width(formatted)<=288,id..": line too wide: "..width(formatted))
    end
end
assert(count==17,"Expected 17 tutorial message records")

local file=assert(io.open("tutorial/script.lua","rb"))
local raw=file:read("*a") file:close()
local staticCount=0
for source in raw:gmatch("ShowMessage%a*%(%[%[(.-)%]%]%)") do
    captured=nil ShowMessage(source)
    assert(captured and captured:sub(1,4)=="@TJ/","Missing translation: "..normalize(source))
    covered[assert(captured:match("^@TJ/([^/]+)/"))]=true
    staticCount=staticCount+1
end
assert(staticCount==15,"Static message extraction missed a call: "..staticCount)
local dynamicCount=0
for template in raw:gmatch('string%.format%((".-")') do
    local decoded=assert(load("return "..template))()
    for _,score in ipairs({0,1,9,10,25}) do
        local source=string.format(decoded,score,25)
        if decoded:sub(1,8)=="TARGETS:" then
            smallCaptured=nil ShowSmallMessage(source)
            assert(smallCaptured and smallCaptured:match("^@TJ/targets/1/"..score.."/25@"))
            covered.targets=true
        else
            captured=nil ShowMessage(source)
            local id=decoded:find("inadequate",1,true) and "score_bad" or "score_good"
            assert(captured and captured:find("@TJ/"..id.."/1/"..score.."@",1,true))
            covered[id]=true
        end
    end
    dynamicCount=dynamicCount+1
end
assert(dynamicCount==3,"Dynamic message extraction missed a call")
for id in pairs(records) do assert(covered[id],"Uncovered translation: "..id) end

for id,record in pairs(records) do
    local values={25,25}
    local text=TeleglitchJP.tutorial_layout(record,values,id=="targets")
    local rows={}
    for line in (text.."\n"):gmatch("(.-)\n") do rows[#rows+1]=line end
    for index,row in ipairs(record.rows) do
        local line=rows[id=="targets" and index or math.floor((index-1)*2*TeleglitchJP.layout.scale+0.00001)+1]
        assert(not line:find("[\128-\255]"),id..": native layout must stay ASCII")
        assert(#line*4>=width(string.format(row,25,25))*TeleglitchJP.layout.scale,id..": width underestimated")
    end
end
ShowMessage("Unknown text") assert(captured=="Unknown text")
levelname="level1"
ShowMessage(records.welcome.source) assert(captured==records.welcome.source)
ShowSmallMessage("TARGETS:7/25") assert(smallCaptured=="TARGETS:7/25")
levelname="tutorial"
local before=calls dofile("teleglitch-jp/tutorial.lua")
ShowMessage(records.welcome.source) assert(calls==before+1,"Wrapper was installed twice")
print("PASS: 17 records; 15 static calls; 3 numeric templates at 0/1/9/10/25; layout; scope; fallback; idempotence")
assert(math.abs(TeleglitchJP.layout.scale-0.7)<0.0001,"Tutorial must use requested 70% size")
