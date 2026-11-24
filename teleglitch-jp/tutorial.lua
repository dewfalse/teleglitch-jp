-- Keep the original tutorial logic and keys; translate only its displayed text.
-- Native dialog layout measures bytes, so pass ASCII layout lines to the game.
-- The renderer replaces each line with its Japanese text at a 12px font size.
TeleglitchJP = TeleglitchJP or {}
if TeleglitchJP.tutorial_loaded then return end

local file, err = io.open("teleglitch-jp/tutorial_messages.tsv", "rb")
if not file then print("JP tutorial: " .. tostring(err) .. "\n") return end

local records, exact, patterns = {}, {}, {}
local textLayout=dofile("teleglitch-jp/layout.lua")
local function normalize(text)
    return (text:gsub("%s+", " "):match("^%s*(.-)%s*$"))
end
local function escapePattern(text)
    return (text:gsub("([%^%$%(%)%%%.%[%]%*%+%-%?])", "%%%1"))
end
local function numberPattern(text)
    local parts, offset = {"^"}, 1
    while true do
        local first, last = text:find("%%d", offset)
        if not first then break end
        parts[#parts+1] = escapePattern(text:sub(offset, first-1))
        parts[#parts+1] = "(%d+)"
        offset = last+1
    end
    parts[#parts+1] = escapePattern(text:sub(offset))
    parts[#parts+1] = "$"
    return table.concat(parts)
end
for line in file:lines() do
    line = line:gsub("\r$", "")
    if line:sub(1,1) ~= "#" and #line > 0 then
        local id, english, japanese = line:match("^([^\t]+)\t([^\t]+)\t(.+)$")
        if id then
            local rows = {}
            japanese = japanese:gsub("\\n", "\n")
            for row in (japanese .. "\n"):gmatch("(.-)\n") do rows[#rows+1] = row end
            local record = {id=id, source=normalize(english), rows=rows}
            records[id] = record
            if english:find("%%d") then
                patterns[#patterns+1] = {pattern=numberPattern(record.source), record=record}
            else
                exact[record.source] = record
            end
        end
    end
end
file:close()

local function findRecord(message)
    local source = normalize(message)
    if exact[source] then return exact[source], {} end
    for _, item in ipairs(patterns) do
        local values = {source:match(item.pattern)}
        if #values > 0 then
            for index, value in ipairs(values) do values[index] = tonumber(value) end
            return item.record, values
        end
    end
end

local layout=textLayout.encode

local originalMessage, originalSmall = ShowMessage, ShowSmallMessage
function ShowMessage(message, ...)
    if levelname == "tutorial" and type(message) == "string" then
        local record, values = findRecord(message)
        if record and record.id ~= "targets" then
            return originalMessage(layout(record, values, false), ...)
        end
    end
    return originalMessage(message, ...)
end
function ShowSmallMessage(message, ...)
    if levelname == "tutorial" and type(message) == "string" then
        local record, values = findRecord(message)
        if record and record.id == "targets" then
            return originalSmall(layout(record, values, true), ...)
        end
    end
    return originalSmall(message, ...)
end

TeleglitchJP.tutorial_loaded = true
TeleglitchJP.tutorial_records = records
TeleglitchJP.tutorial_layout = layout
