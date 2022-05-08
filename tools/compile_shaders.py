import sys
import os
import argparse
import subprocess

arg_parser = argparse.ArgumentParser(description='')
arg_parser.add_argument('--outdir', type=str, nargs='?',
                        help='binaries output dir')
arg_parser.add_argument('--bytecodes', nargs='+', help='output bytecodes')
arg_parser.add_argument('--shader', nargs='?', help='input shader sources')

if __name__ == "__main__":
    args = arg_parser.parse_args(sys.argv[1:])

    spirv = 0
    dxil = 0
    msl = 0

    current_dir = os.path.dirname(os.path.abspath(__file__))

    print(args.shader)

    for bytecode in args.bytecodes:
        if bytecode == 'spirv':
            spirv = 1
        if bytecode == 'dxil':
            dxil = 1
        if bytecode == 'msl':
            spirv = 1
            msl = 1

    if not os.path.exists(args.outdir):
        os.makedirs(args.outdir)

    if spirv:
        if not os.path.exists(args.outdir + '/vulkan'):
            os.mkdir(args.outdir + '/vulkan')

        shader_name = args.shader
        output_shader_name = args.outdir + '/vulkan/' + \
            os.path.basename(shader_name).replace('hlsl', 'bin')
        stage = ''

        if 'vert' in shader_name:
            stage = 'vert'
        if 'frag' in shader_name:
            stage = 'frag'
        if 'comp' in shader_name:
            stage = 'comp'

        subprocess.call(['glslangValidator', '-e', 'main', '-V', '-I' + current_dir,
                        shader_name, '-o', output_shader_name])

    if dxil:
        if not os.path.exists(args.outdir + '/d3d12'):
            os.mkdir(args.outdir + '/d3d12')

        shader_name = args.shader
        output_shader_name = args.outdir + '/d3d12/' + \
            os.path.basename(shader_name).replace('hlsl', 'bin')
        stage = ''

        if 'vert' in shader_name:
            stage = 'vs_6_0'
        if 'frag' in shader_name:
            stage = 'ps_6_0'
        if 'comp' in shader_name:
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