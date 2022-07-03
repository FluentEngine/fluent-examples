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

commons.example("light")
	files
	{
		"common/ui_pass.h",
		"common/ui_pass.c",
		"common/entry_point.h",

		"light/main.c",
		"light/main_pass.h",
		"light/main_pass.c",
		"light/shader_main_vert_spirv.c",
		"light/shader_main_frag_spirv.c",
	}
