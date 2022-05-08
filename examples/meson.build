examples = [ 
    '00_minimal',
    '01_hello_triangle',
    '02_load_texture',
    '03_compute'
]

shaders = {
    examples[1] : files([
        'shaders/01_hello_triangle/main.vert.hlsl',
        'shaders/01_hello_triangle/main.frag.hlsl',
    ]),
    examples[2] : files([
        'shaders/02_load_texture/main.vert.hlsl',
        'shaders/02_load_texture/main.frag.hlsl',
    ]),
    examples[3] : files([
        'shaders/03_compute/main.comp.hlsl'
    ])
}

foreach example : examples
    srcs = files([
        example/'main.cpp',
        'common/sample.hpp'
    ])

    compiled_shaders = []

    shader_generator = generator(prog_python,
      output        : [ '@BASENAME@.bin' ],
      arguments     : [ meson.current_source_dir()/'../tools/compile_shaders.py', '--outdir', meson.current_source_dir()/'../build'/'shaders'/example, '--bytecodes', bytecodes, '--shader', '@INPUT@' ]
    )

    if example in shaders
        compiled_shaders = shader_generator.process(shaders[example])
    endif

    executable(example, srcs, compiled_shaders,
        dependencies	: fluent_engine_dep,
        cpp_args		: compile_args,
    )
endforeach
