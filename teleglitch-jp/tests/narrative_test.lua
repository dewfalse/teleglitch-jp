dofile("lua/story.lua")
dofile("lua/levelnames.lua")
dofile("lua/cfunctions.lua")
local originalKeys,originalTransitions,originalNames={},{},{}
for key in pairs(storytable) do originalKeys[key]=true end
for key in pairs(leveltexts) do originalTransitions[#originalTransitions+1]=key end
for key,value in pairs(levelnames) do originalNames[key]=value end
local captured,small,calls=nil,nil,0
ShowMessage=function(text)captured=text calls=calls+1 end
ShowSmallMessage=function(text)small=text end
levelname="level1"
dofile("teleglitch-jp/tutorial.lua")
dofile("teleglitch-jp/narratives.lua")
local records=TeleglitchJP.narrative_records
local stories,transitions,names,notices,literals,maxRows=0,0,0,0,0,0
local deaths=0
for id,record in pairs(records) do
    maxRows=math.max(maxRows,#record.rows)
    assert(((#record.rows-1)*14+12)*TeleglitchJP.layout.scale<212,"Text too tall: "..id)
    local encoded=TeleglitchJP.layout.encode(record,{},false)
    assert(not encoded:find("[\128-\255]"),"Non-ASCII native layout: "..id)
    local nativeRows={}
    for line in (encoded.."\n"):gmatch("(.-)\n") do nativeRows[#nativeRows+1]=line end
    for i,row in ipairs(record.rows) do
        local position=math.floor((i-1)*2*TeleglitchJP.layout.scale+0.00001)+1
        local actual=nativeRows[position]
        if #row>0 then
            assert(actual and actual:find("@TJ/"..id.."/"..i.."@",1,true),"Missing packed row: "..id)
            assert(#actual>=TeleglitchJP.layout.columns(row),"Width mismatch: "..id)
        else assert(actual=="","Paragraph gap lost: "..id) end
    end
    if id:sub(1,1)=="s" then
        stories=stories+1
        assert(originalKeys[record.key],"Unknown original story key: "..record.key)
        local body=storytable[record.key]
        local separator=assert(encoded:find("@TJ/"..id.."/3@",1,true))
        assert(record.rows[2]=="" and body==encoded:sub(separator),"Catalog body mismatch: "..id)
        ShowMessage(record.key.." \n\n"..body)
        assert(captured==encoded,"Terminal title or text mismatch: "..id)
    elseif id:sub(1,1)=="t" then
        transitions=transitions+1
        local found=false
        for key,value in pairs(leveltexts) do
            local encodedKey=type(key)=="table" and key[1]..">"..key[2] or key
            if encodedKey==record.key then assert(value==encoded) found=true end
        end
        assert(found,"Transition was not translated: "..id)
    elseif id:sub(1,1)=="d" then
        deaths=deaths+1
        local index=assert(tonumber(id:sub(2)))
        local random=math.random
        math.random=function(count) assert(count==58) return index end
        local selected=GetRandomDeathText()
        math.random=random
        assert(selected==encoded,"Untranslated or mismatched death message: "..id)
    elseif record.key:sub(1,8)=="literal:" then
        literals=literals+1
        ShowMessage(record.key:sub(9))
        assert(captured==encoded,"Stage-specific message not translated: "..id)
    elseif record.key:sub(1,5)=="name:" then
        names=names+1
        local key=record.key:sub(6)
        ShowSmallMessage(key..": "..originalNames[key])
        assert(small:find("@TJ/"..id.."/1@",1,true),"Stage name missing: "..id)
    else
        notices=notices+1
        ShowSmallMessage(record.key:sub(9))
        assert(small:find("@TJ/"..id.."/1@",1,true),"Notification missing: "..id)
    end
end
assert(stories==131 and transitions==19 and names==15 and notices==9 and literals==15,"Coverage counts changed")
assert(deaths==58 and #deathtexts==58,"Death message coverage changed")
print("PASS: all 58 random death message selections return their Japanese rows")
local keyCount,transitionCount=0,0
for key in pairs(storytable) do assert(originalKeys[key],"Internal discovery key changed") keyCount=keyCount+1 end
for _,key in ipairs(originalTransitions) do assert(leveltexts[key]) transitionCount=transitionCount+1 end
assert(keyCount==131 and transitionCount==19,"Original keys were removed")
for recipe,title in pairs(replacetitles) do assert(storytable[title],"Broken recipe story link: "..recipe.." -> "..title) end
ShowMessage("Untranslated unrelated message") assert(captured=="Untranslated unrelated message")
ShowSmallMessage("Untranslated unrelated notification") assert(small=="Untranslated unrelated notification")
levelname="tutorial" ShowSmallMessage("TARGETS:9/25") assert(small:find("@TJ/targets/1/9/25@",1,true))
-- StartFrom updates levelname after the native call. Exercise the real
-- LevelStart while the previous stage is still visible to the dialog hooks.
local openingDialogs={}
local function noop() end
local openingEnv={
    scripttable={}, EnableInput=noop, EnableHUD=noop, ShowPlayer=noop,
    Wait=noop, SetDarkness=noop,
    ShowMessage=function(message)
        ShowMessage(message)
        openingDialogs[#openingDialogs+1]=captured
    end,
}
assert(loadfile("level1/script.lua","t",openingEnv))()
openingEnv.scripttable.LevelStart()
assert(#openingDialogs==1 and openingDialogs[1]==TeleglitchJP.layout.encode(records.p001,{},false),
    "Level 1 opening after tutorial was not translated")
local before=calls dofile("teleglitch-jp/narratives.lua")
levelname="level1" ShowMessage("Untranslated unrelated message") assert(calls==before+1)
local popups,notifications=0,0
for stage in pairs(originalNames) do
    local file=assert(io.open(stage.."/script.lua","rb"))
    local source=file:read("*a") file:close()
    levelname=stage
    for text in source:gmatch("ShowMessage%(%s*%[%[(.-)%]%]%)") do
        ShowMessage(text) assert(captured:sub(1,4)=="@TJ/","Untranslated stage popup: "..stage)
        popups=popups+1
    end
    for quoted in source:gmatch('ShowMessage%(%s*("[^"]*")%s*%)') do
        ShowMessage(assert(load("return "..quoted))())
        assert(captured:sub(1,4)=="@TJ/","Untranslated quoted popup: "..stage)
        popups=popups+1
    end
    for quoted in source:gmatch('ShowSmallMessage%(%s*("[^"]*")%s*%)') do
        ShowSmallMessage(assert(load("return "..quoted))())
        assert(small:sub(1,4)=="@TJ/","Untranslated notification: "..stage)
        notifications=notifications+1
    end
    for quoted,key in source:gmatch('ShowMessage%(%s*("[^"]*")%s*%.%.storytable%["(.-)"%]%)') do
        ShowMessage(assert(load("return "..quoted))()..assert(storytable[key]))
        assert(captured:sub(1,4)=="@TJ/","Untranslated composed popup: "..stage)
        popups=popups+1
    end
end
local file=assert(io.open("lua/ddeweapons.lua","rb"))
local dde=file:read("*a") file:close()
local packages=assert(dde:match("startitempackages%s*=(.-)function GetStartingItems"))
local starts=0
levelname="level1"
for text in packages:gmatch("text%s*=%s*%[%[(.-)%]%]") do
    ShowMessage(text)
    assert(captured:sub(1,4)=="@TJ/","Untranslated DDE starting item narrative")
    starts=starts+1
end
assert(starts==8,"DDE starting narrative count changed")
print(string.format("PASS: %d story records; 18 transitions + ending; %d names; %d notices; %d literal texts; 15 stage scripts (%d popup calls / %d notifications); %d DDE start texts; longest=%d rows; 70%% layout",stories,names,notices,literals,popups,notifications,starts,maxRows))
