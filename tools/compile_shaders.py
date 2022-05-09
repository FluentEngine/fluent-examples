import sys
import os
import argparse
import subprocess

arg_parser = argparse.ArgumentParser(description='')
arg_parser.add_argument('--input', nargs='?', help='input shader')
arg_parser.add_argument('--output', type=str, nargs='?',
									help='compiled shader')
arg_parser.add_argument('--bytecodes', nargs='+', help='output bytecodes')

if __name__ == "__main__":
	args = arg_parser.parse_args(sys.argv[1:])

	current_dir = os.path.dirname(os.path.abspath(__file__))

	spirv = 0
	dxil = 0
	msl = 0

	for bytecode in args.bytecodes:
		if bytecode == 'spirv':
			spirv = 1
		if bytecode == 'dxil':
			dxil = 1
		if bytecode == 'msl':
			msl = 1

	if spirv:
		array_name = os.path.basename(args.input).replace('.', '_')
		
		stage = ''
		if 'vert' in args.input:
			stage = 'vert'
		elif 'frag' in args.input:
			stage = 'frag'
		elif 'comp' in args.input:
			stage = 'comp'
			
		subprocess.call(['glslangValidator', '-S', stage, '-D', '-e', 'main', '-V', '-I' + current_dir,
						args.input, '--vn', 'shader_' + array_name.replace('_ft', ''), '-o', args.output ])

	if dxil:
		if not os.path.exists(args.outdir + '/d3d12'):
			os.mkdir(args.outdir + '/d3d12')

		shader_name = args.shader
		output_shader_name = args.outdir + '/d3d12/' + \
		os.path.basename(shader_name).replace('hlsl', 'bin')

		stage = ''

		if 'vert' in shader_name:
			stage = 'vs_6_0'
		elif 'frag' in shader_name:
			stage = 'ps_6_0'
		elif 'comp' in shader_name:
			stage = 'cs_6_0'

		subprocess.call(['dxc', '-E', 'main', '-T', stage, '-Fo',
						output_shader_name, shader_name])

	if msl:
		stage = ''
		if 'vert' in args.input:
			stage = 'vert'
		elif 'frag' in args.input:
			stage = 'frag'
		elif 'comp' in args.input:
			stage = 'comp'

		spv = args.output.replace('.h', '.spv')
		
		subprocess.call(['glslangValidator', '-S', stage, '-D', '-e', 'main', '-V', '-I' + current_dir,
				args.input, '-o',  spv ])

		subprocess.call(['spirv-cross', spv, '--msl', '--output', spv.replace('.spv', '.metal')])
		subprocess.call([ 'xcrun', '-sdk', 'macosx', 'metal', '-c', spv.replace('.spv', '.metal'), '-o', spv.replace('.spv', '.air')])
		subprocess.call([ 'xcrun', '-sdk', 'macosx', 'metallib', spv.replace('.spv', '.air'), '-o', spv.replace('.spv', '') ])
		print('xxd' + '-i' + spv.replace('.spv', '') + '>' + spv.replace('.' + stage + '.spv', '.h'))
		f = open(spv.replace('.spv', '.h'), 'w')
		os.chdir(os.path.dirname(spv.replace('.spv', '')))
		subprocess.call([ 'xxd', '-i', os.path.basename(spv.replace('.spv', '')) ], stdout=f)

