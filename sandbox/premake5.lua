project "sandbox"
    kind "WindowedApp"
    language "C++"

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
    }

    files {
        "main.c",

        "main.vert.ft",
        "main.frag.ft"
    }

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
		
