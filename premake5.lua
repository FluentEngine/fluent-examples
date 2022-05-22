workspace "fluent-examples"
    targetdir "build/%{prj.name}"
    objdir "build/%{prj.name}"
    location "build"
    configurations { "release", "debug" }

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
include "examples/premake5.lua"
