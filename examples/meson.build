examples = [ 
    '00_minimal',
    '01_hello_triangle',
    '02_load_texture',
    '03_compute'
]

shaders = {
    examples[1] : files([
        'shaders/01_hello_triangle/main.vert.ft',
        'shaders/01_hello_triangle/main.frag.ft',
    ]),
    examples[2] : files([
        'shaders/02_load_texture/main.vert.ft',
        'shaders/02_load_texture/main.frag.ft',
    ]),
    examples[3] : files([
        'shaders/03_compute/main.comp.ft'
    ])
}

foreach example : examples
    srcs = files([
        example/'main.cpp',
        'common/sample.hpp'
    ])

    compiled_shaders = []

    if example in shaders
        compiled_shaders = ft_shader_gen.process(shaders[example])
    endif
    
    executable(example, srcs, compiled_shaders,
        dependencies	: fluent_engine_dep,
        cpp_args		: compile_args,
    )
endforeach
