project('fluent-examples', [ 'c', 'cpp' ], default_options : ['default_library=static', 'c_std=c17', 'cpp_std=c++17'] )

compile_args = []

fluent = subproject('fluent')
fluent_engine_dep = fluent.get_variable('fluent_engine_dep')

prog_python = import('python').find_installation('python3')

shaders = files([
    'sandbox/shaders/gbuffer.vert.hlsl',
    'sandbox/shaders/gbuffer.frag.hlsl',
    'sandbox/shaders/deffered_shading.vert.hlsl',
    'sandbox/shaders/deffered_shading.frag.hlsl'
])

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

sub = 'sandbox'

shader_generator = generator(prog_python,
  output        : [ '@BASENAME@.bin' ],
  arguments     : [ meson.current_source_dir()/'tools/compile_shaders.py', '--outdir', meson.current_source_dir()/'build'/'shaders'/sub , '--bytecodes', bytecodes, '--shader', '@INPUT@' ]
)

executable('sandbox', [ 'sandbox/main.cpp', 'sandbox/render_graph.hpp', 'sandbox/render_graph.cpp' ], shader_generator.process(shaders),
  dependencies	: fluent_engine_dep,
  cpp_args      : compile_args,
)

subdir('examples')
