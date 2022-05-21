workspace "fluent-examples"
    targetdir "build/%{prj.name}"
    objdir "build/%{prj.name}"
    location "build"
    configurations { "release", "debug" }

    buildoptions {}
    filter { "configurations:debug" }
        buildoptions { "-g" }
        defines { "FLUENT_DEBUG" }
    filter { "configurations:release" }
        buildoptions { "-O3" }

-- TODO: make option
if (true)
then
    git_url_prefix = "git@github.com:"
else
    git_url_prefix = "https://github.com/"
end

if (not os.isdir("./fluent"))
then
    fluent_engine_repo = git_url_prefix .. "FluentEngine/fluent.git ./fluent"
    os.execute("git clone " .. fluent_engine_repo )
end

include "fluent/fluent-engine.lua"
include "sandbox/premake5.lua"
