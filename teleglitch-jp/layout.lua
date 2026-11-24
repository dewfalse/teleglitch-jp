TeleglitchJP=TeleglitchJP or {}
if TeleglitchJP.layout then return TeleglitchJP.layout end
local layout={scale=0.70}
local file=io.open("teleglitch-jp/text_settings.ini","rb")
if file then
    local value=tonumber(file:read("*a"):match("body_scale%s*=%s*([%d%.]+)"))
    file:close()
    if value and value>=0.5 and value<=1.5 then layout.scale=value end
end
function layout.lines(text)
    local rows={}
    for row in (text:gsub("\\n","\n").."\n"):gmatch("(.-)\n") do rows[#rows+1]=row end
    return rows
end
function layout.columns(text)
    local units=0
    for ch in text:gmatch("[%z\1-\127\194-\244][\128-\191]*") do
        units=units+(#ch==1 and 1 or 2)
    end
    return math.ceil(units*1.5*layout.scale)
end
function layout.encode(record,values,small)
    values=values or {}
    local lines={}
    for index,row in ipairs(record.rows) do
        local nativeIndex=small and index-1 or math.floor((index-1)*2*layout.scale+0.00001)
        while #lines<=nativeIndex do lines[#lines+1]="" end
        if #row>0 then
            local text=(#values>0 and row:find("%%d")) and string.format(row,table.unpack(values)) or row
            local token="@TJ/"..record.id.."/"..index
            for _,value in ipairs(values) do token=token.."/"..value end
            token=token.."@"
            lines[nativeIndex+1]=token..string.rep(" ",math.max(0,layout.columns(text)-#token))
        end
    end
    if not small then
        local count=math.ceil(((#record.rows-1)*14+12)*layout.scale/7)
        while #lines<count do lines[#lines+1]="" end
    end
    return table.concat(lines,"\n")
end
TeleglitchJP.layout=layout
return layout
