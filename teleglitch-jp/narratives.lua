TeleglitchJP=TeleglitchJP or {}
if TeleglitchJP.narratives_loaded then return end
local textLayout=dofile("teleglitch-jp/layout.lua")
local file,err=io.open("teleglitch-jp/narrative_messages.tsv","rb")
if not file then print("JP stage text: "..tostring(err).."\n") return end
local records,small,literals={},{},{}
local function normalize(text) return text:gsub("%s+"," "):match("^%s*(.-)%s*$") end
local originalStories={}
for line in file:lines() do
    line=line:gsub("\r$","")
    if #line>0 and line:sub(1,1)~="#" then
        local id,key,japanese=line:match("^([^\t]+)\t([^\t]+)\t(.+)$")
        if id then
            records[id]={id=id,key=key,rows=textLayout.lines(japanese)}
        end
    end
end
file:close()
for id,record in pairs(records) do
    local encoded=textLayout.encode(record,{},false)
    if id:sub(1,1)=="s" and storytable and storytable[record.key] then
        originalStories[record.key]=storytable[record.key]
        -- The catalog draws its own heading. Keep the body tokens' original
        -- row numbers for the renderer; ShowMessage uses the complete record.
        local separator=1
        while record.rows[separator] and record.rows[separator]~="" do separator=separator+1 end
        local firstBodyRow=separator+1
        while record.rows[firstBodyRow]=="" do firstBodyRow=firstBodyRow+1 end
        local bodyStart=encoded:find("@TJ/"..id.."/"..firstBodyRow.."@",1,true)
        storytable[record.key]=bodyStart and encoded:sub(bodyStart) or encoded
    elseif id:sub(1,1)=="t" and leveltexts then
        if record.key=="gameend" then leveltexts.gameend=encoded
        else
            local old,new=record.key:match("^([^>]+)>([^>]+)$")
            for key in pairs(leveltexts) do
                if type(key)=="table" and key[1]==old and key[2]==new then leveltexts[key]=encoded end
            end
        end
    elseif id:sub(1,1)=="d" and deathtexts then
        -- The game picks a random array slot. Check its opening words as
        -- well, so a different message in that slot keeps its own text.
        local index=tonumber(id:sub(2))
        local source=deathtexts[index]
        if type(source)=="string" and normalize(source):sub(1,#record.key)==record.key then
            deathtexts[index]=encoded
        end
    elseif id:sub(1,1)=="p" then
        literals[normalize(record.key:sub(9))]=record
    elseif id:sub(1,1)=="n" then
        local name=record.key:match("^name:(.+)$")
        local message=record.key:match("^message:(.+)$")
        if name and levelnames and levelnames[name] then small[name..": "..levelnames[name]]=record
        elseif message then small[message]=record end
    end
end
-- The shipped recipe has a capitalization typo; retain the canonical story ID.
if replacetitles and replacetitles["RECIPE:rifle2x"]=="Twin heavy rifle" and storytable["Twin Heavy Rifle"] then
    replacetitles["RECIPE:rifle2x"]="Twin Heavy Rifle"
end
local originalMessage,originalSmall=ShowMessage,ShowSmallMessage
function ShowMessage(message,...)
    -- LevelStart can show a dialog before StartFrom updates levelname.
    -- Identify narrative dialogs by their registered text or story token.
    if type(message)=="string" then
        local literal=literals[normalize(message)]
        if literal then return originalMessage(textLayout.encode(literal,{},false),...) end
        -- Generated terminal scripts prepend the original English key. The full
        -- translated record already includes its Japanese title, so use it once.
        local id=message:match("@TJ/(s%d+)/")
        if id and records[id] then return originalMessage(textLayout.encode(records[id],{},false),...) end
    end
    return originalMessage(message,...)
end
function ShowSmallMessage(message,...)
    if levelname~="tutorial" and small[message] then
        return originalSmall(textLayout.encode(small[message],{},true),...)
    end
    return originalSmall(message,...)
end
TeleglitchJP.narratives_loaded=true
TeleglitchJP.narrative_records=records
TeleglitchJP.original_stories=originalStories
