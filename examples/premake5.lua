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

	sysincludedirs {
		"../deps/fluent/sources",
		"../deps/fluent/third_party",
	}

	fluent_engine.link()
end

commons.example("light")
	files
	{
		"light/main.c",
		"light/ui_pass.h",
		"light/ui_pass.c",
		"light/main_pass.h",
		"light/main_pass.c",
		"light/shaders/shader_pbr_vert_spirv.c",
		"light/shaders/shader_pbr_frag_spirv.c",
		"light/shaders/shader_eq_to_cubemap_comp_spirv.c",
		"light/shaders/shader_skybox_vert_spirv.c",
		"light/shaders/shader_skybox_frag_spirv.c",
		"light/shaders/shader_brdf_comp_spirv.c",
		"light/shaders/shader_irradiance_comp_spirv.c",
		"light/shaders/shader_specular_comp_spirv.c",
	}

	includedirs 
	{
		"light/shaders"
	}