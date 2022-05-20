project('fluent-examples', [ 'c', 'cpp' ], 
	default_options : ['default_library=static', 'c_std=c17', 'cpp_std=c++17'] )

compile_args = [
	'-fno-rtti',
	'-fno-exceptions'
]

if get_option('buildtype') == 'debug'
    compile_args += [ '-g', '-DFLUENT_DEBUG' ]
else
    compile_args += [ '-O3' ]
endif

subdir('sandbox')
