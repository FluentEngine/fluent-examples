glslangValidator -V brdf.comp.glsl -o shader_brdf_comp_spirv
xxd -i shader_brdf_comp_spirv > shader_brdf_comp_spirv.c
rm shader_brdf_comp_spirv

glslangValidator -V eq_to_cubemap.comp.glsl -o shader_eq_to_cubemap_comp_spirv
xxd -i shader_eq_to_cubemap_comp_spirv > shader_eq_to_cubemap_comp_spirv.c
rm shader_eq_to_cubemap_comp_spirv

glslangValidator -V irradiance.comp.glsl -o shader_irradiance_comp_spirv
xxd -i shader_irradiance_comp_spirv > shader_irradiance_comp_spirv.c
rm shader_irradiance_comp_spirv

glslangValidator -V pbr.vert.glsl -o shader_pbr_vert_spirv
xxd -i shader_pbr_vert_spirv > shader_pbr_vert_spirv.c
rm shader_pbr_vert_spirv

glslangValidator -V pbr.frag.glsl -o shader_pbr_frag_spirv
xxd -i shader_pbr_frag_spirv > shader_pbr_frag_spirv.c
rm shader_pbr_frag_spirv

glslangValidator -V skybox.frag.glsl -o shader_skybox_frag_spirv
xxd -i shader_skybox_frag_spirv > shader_skybox_frag_spirv.c
rm shader_skybox_frag_spirv

glslangValidator -V skybox.vert.glsl -o shader_skybox_vert_spirv
xxd -i shader_skybox_vert_spirv > shader_skybox_vert_spirv.c
rm shader_skybox_vert_spirv

glslangValidator -V specular.comp.glsl -o shader_specular_comp_spirv
xxd -i shader_specular_comp_spirv > shader_specular_comp_spirv.c
rm shader_specular_comp_spirv
