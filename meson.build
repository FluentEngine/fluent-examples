project('fluent-examples', [ 'c', 'cpp' ], default_options : ['default_library=static', 'c_std=c17', 'cpp_std=c++17'] )

compile_args = []

fluent = subproject('fluent')
fluent_engine_dep = fluent.get_variable('fluent_engine_dep')

if get_option('buildtype') == 'debug'
    compile_args += [ '-g', '-DFLUENT_DEBUG' ]
else
    compile_args += [ '-O3' ]
endif

prog_python = import('python').find_installation('python3')

bytecodes = []
if dependency('vulkan', required: false).found()
    bytecodes += [ 'spirv' ]
endif

if dependency('d3d12', required: false).found()
    bytecodes += [ 'dxil' ]
endif

if dependency('metal', required: false).found()
    bytecodes += [ 'msl' ]
endif

py = find_program('python3')

ft_shader_args = [
    meson.current_source_dir()/'tools/compile_shaders.py',
	'--input', '@INPUT@',
	'--output', '@OUTPUT@',
	'--bytecodes', 'spirv'
]

ft_shader_gen = generator(
    py,
	output    : [ 'shader_@BASENAME@.h' ],
	arguments : ft_shader_args )

subdir('examples')
subdir('sandbox')
