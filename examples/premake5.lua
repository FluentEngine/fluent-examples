local commons = {}

commons.example = function(name)
project(name)
    kind "WindowedApp"
    language "C++"

	filter { "configurations:debug" }
        symbols "On"
        optimize "Off"
        defines { "DEBUG", "FLUENT_DEBUG" }
    filter { "configurations:release" }
		symbols "Off"
		optimize "Speed"
        defines { "NDEBUG" }
	filter {}
	
	includedirs 
	{
        "../deps/fluent/sources",
	}
	
	sysincludedirs 
	{
        "../deps/fluent/sources/third_party/",
    }
    
	links 
	{ 
        "ft_renderer", 
        "ft_os", 
        "ft_log"
    }

    fluent_engine.link()

    filter { "system:linux" }
        links 
        {
            "m"
        }
    filter { "system:windows" }
        entrypoint "mainCRTStartup"
    filter { }
end

commons.compile_shaders = function(ex)
    filter 'files:**.ft'
        buildmessage 'Compiling shader %{file.relpath}'

        prebuildcommands 
        {
            'python3 ../tools/compile_shaders.py --input ' .. path.getabsolute("%{file.relpath}") .. ' --output ' .. path.getabsolute(ex .."/%{file.basename}.h") .. ' --bytecodes spirv'
        }

        buildcommands 
        {
            'python3 ../tools/compile_shaders.py --input ' .. path.getabsolute("%{file.relpath}") .. ' --output ' .. path.getabsolute(ex .. "/%{file.basename}.h") .. ' --bytecodes spirv'
        }

        buildoutputs 
        { 
			path.getabsolute(ex .."/%{file.basename}.h")
		}
	filter {}
end

commons.example("sandbox")
    files 
    {
        "sandbox/main.c",

        "sandbox/main.vert.ft",
        "sandbox/main.frag.ft"
    }

	commons.compile_shaders("sandbox")

commons.example("render_graph")
	files 
	{
		"render_graph/main.c",
	}
	
-- commons.example("test_wsi")
--     files 
--     {
--         "test_wsi/test_wsi.c",

-- 		"test_wsi/main.vert.ft",
-- 		"test_wsi/main.frag.ft"
-- 	}
	
-- 	sysincludedirs
-- 	{	
-- 		vulkan_include_directory
-- 	}
	
-- 	links 
-- 	{ 
--         "glfw"
--     }
	
-- 	commons.compile_shaders("test_wsi")
