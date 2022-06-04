workspace "fluent-examples"
    targetdir "build/%{prj.name}"
    objdir "build/%{prj.name}"
    location "build"
    configurations { "release", "debug" }

    architecture "x64"

    root_directory = path.getabsolute(".")

    -- TODO: make option
    if (true)
    then
        git_url_prefix = "git@github.com:"
    else
        git_url_prefix = "https://github.com/"
    end

    if (not os.isdir(root_directory .. "/deps/fluent"))
    then
        fluent_engine_repo = git_url_prefix .. "FluentEngine/fluent.git " .. root_directory .. "/deps/fluent"
        os.execute("git clone " .. fluent_engine_repo )
    end

    include "deps/fluent/fluent-engine.lua"
    include "examples/premake5.lua"
