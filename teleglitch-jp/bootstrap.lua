-- Load the optional native font renderer without making startup depend on it.
do
    local loader, err = package.loadlib("teleglitch-jp/teleglitch_jp.dll", "luaopen_teleglitch_jp")
    if loader then
        local ok, result = pcall(loader)
        if not ok then
            print("JP renderer: " .. tostring(result) .. "\n")
        elseif result == true then
            local tutorial_ok, tutorial_err = pcall(dofile, "teleglitch-jp/tutorial.lua")
            if not tutorial_ok then print("JP tutorial: " .. tostring(tutorial_err) .. "\n") end
            local stage_ok, stage_err = pcall(dofile, "teleglitch-jp/narratives.lua")
            if not stage_ok then print("JP stage text: " .. tostring(stage_err) .. "\n") end
        end
    else
        print("JP renderer unavailable: " .. tostring(err) .. "\n")
    end
end
