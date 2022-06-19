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
	
	defines { "MODEL_FOLDER=" .. '"' .. path.getabsolute("../../glTF-Sample-Models/2.0/") .. '"' }

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
            'python3 ' .. path.getabsolute('../tools/compile_shaders.py') .. ' --input ' .. '%{file.relpath} --output ' .. path.getabsolute(ex ..'/%{file.basename}.h') .. ' --bytecodes spirv'
        }

        buildcommands 
        {
            'python3 ' .. path.getabsolute('../tools/compile_shaders.py') .. ' --input ' .. '%{file.relpath} --output ' .. path.getabsolute(ex ..'/%{file.basename}.h') .. ' --bytecodes spirv'
        }

        buildoutputs 
        { 
			path.getabsolute(ex .."/%{file.basename}.h")
		}
	filter {}
end

commons.example("render_graph")
	files 
	{
		"render_graph/main.c",
		"render_graph/main_pass.h",
		"render_graph/main_pass.c",
		"render_graph/shader_main_vert_spirv.c",
		"render_graph/shader_main_frag_spirv.c",

		"render_graph/main.vert.ft",
		"render_graph/main.frag.ft",
	}
	
