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
			spirv = 1
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
		if not os.path.exists(args.outdir + '/metal'):
			os.mkdir(args.outdir + '/metal')

		shader_name = args.shader
		output_shader_name = args.outdir + '/metal/' + \
		os.path.basename(shader_name).replace('hlsl', 'bin')

		subprocess.call(['spirv-cross', args.outdir + '/vulkan/' + os.path.basename(shader_name).replace('hlsl', 'bin'), '--msl', '--output', output_shader_name.replace('bin', 'metal')])
		subprocess.call([ 'xcrun', '-sdk', 'macosx', 'metal', '-c', output_shader_name.replace('bin', 'metal'), '-o', output_shader_name.replace('bin', 'air')])
		subprocess.call([ 'xcrun', '-sdk', 'macosx', 'metallib', output_shader_name.replace('bin', 'air'), '-o', output_shader_name ])

