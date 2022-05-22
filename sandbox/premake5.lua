local commons = {}

commons.opts = function()
    kind "WindowedApp"
    language "C++"
	
	filter { "configurations:debug" }
        symbols "On"
        defines { "FLUENT_DEBUG" }
    filter { "configurations:release" }
		symbols "Off"
		optimize "Speed"
		
	includedirs {
        "../fluent/sources",
        "../fluent/sources/third_party/",
    }
    
	links { 
        "ft_renderer", 
        "ft_os", 
        "ft_log", 
        "SDL2",
        "hashmap_c",
        "spirv_reflect",
        "tiny_image_format",
        "vk_mem_alloc",
        "volk",
        "m"
    }
end

commons.shaders = function()
    filter 'files:**.ft'
        buildmessage 'Compiling shader %{file.relpath}'

        prebuildcommands {
            'python3 ../tools/compile_shaders.py --input ' .. path.getabsolute("%{file.relpath}") .. ' --output ' .. path.getabsolute("%{file.basename}.h") .. ' --bytecodes spirv'
        }

        buildcommands {
            'python3 ../tools/compile_shaders.py --input ' .. path.getabsolute("%{file.relpath}") .. ' --output ' .. path.getabsolute("%{file.basename}.h") .. ' --bytecodes spirv'
        }

        buildoutputs { 
			path.getabsolute("%{file.basename}.h")
		}
end

project "sandbox"

	commons.opts()
	
    files {
        "main.c",

        "main.vert.ft",
        "main.frag.ft"
    }

	commons.shaders()

project "test_wsi"
	
	commons.opts()

    files {
        "test_wsi.c",

        "main.vert.ft",
        "main.frag.ft"
    }
	
	links {		
		"glfw"	
	}
	
	commons.shaders()
