shaders = files([
    'shaders/gbuffer.vert.ft',
	'shaders/gbuffer.frag.ft',
	'shaders/deffered_shading.vert.ft',
	'shaders/deffered_shading.frag.ft'
])

executable('sandbox', [ 
		'main.cpp', 
		'render_graph.hpp', 
		'render_graph.cpp',
		'model.hpp',
		ft_shader_gen.process(shaders) ],
    dependencies	: fluent_engine_dep,
	cpp_args		: compile_args,
	install			: true
)
