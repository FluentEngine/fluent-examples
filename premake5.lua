workspace "fluent-examples"

	newoption
	{
		trigger = "build_directory",
		description = "build directory"
	}
	
	build_directory = "build/"
	
	if _OPTIONS["build_directory"] ~= nil then
		build_directory = _OPTIONS["build_directory"] .. "/"
	end

	targetdir (build_directory .. "/%{prj.name}")
	objdir (build_directory .."/%{prj.name}")
	location (build_directory)
	configurations { "release", "debug" }
	
	filter { "system:windows" }
		architecture "x64"
		staticruntime "On"
		filter { }

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

	if (not os.isdir(path.join(root_directory, "../glTF-Sample-Models")))
	then
		os.execute("git clone https://github.com/KhronosGroup/glTF-Sample-Models.git " .. root_directory .. "../glTF-Sample-Models")
	end

	include ("deps/fluent/fluent-engine.lua")
	include ("examples/premake5.lua")
