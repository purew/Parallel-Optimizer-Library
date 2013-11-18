#! /usr/bin/env python
# encoding: utf-8
# Build-script for Parallell Optimizer Library


# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='Parallel-Optimizer-Library'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'


# constants
str_release = 'release'
str_debug = 'debug'

def options(opt):
	opt.load('compiler_cxx')

def configure(cfg):	
	# Common options
	cfg.load('gxx')
	cfg.env.append_value('CXXFLAGS', ['-std=c++11','-Wall'])
	cfg.env.append_value('INCLUDES', ['include'])
	
	# Special targets 
	
	cfg.setenv(str_debug, cfg.env.derive())
	cfg.env.append_value('CXXFLAGS', ['-g'])
	
	cfg.setenv(str_release, cfg.env.derive())
	cfg.env.append_value('CXXFLAGS', ['-O3'])
	

def build(bld):
	
	print bld.variant
	if not bld.variant in [str_release,str_debug]:
		bld.fatal('Call \n    "./waf build_debug" or \n    "./waf clean_debug"\nor the equivalent for release')

	
	ext_paths = ['/usr/lib/i386-linux-gnu', '/usr/lib/x86_64-linux-gnu']
	bld.read_shlib('pthread', paths = ext_paths)
	
	bld.stlib(
		source='src/Optimizer.cpp src/ParticleSwarmOptimization.cpp', 
		target='pao',
		use='pthread')
	
	bld.program(
		source='example/rosenbrock.cpp', 
		target='rosenbrock', 
		use='pao')
	
	# Generate README.md for Github
	bld(
		rule='sed -e \'/END OF DOCUMENTATION/,$$d\' ${SRC} | tail -n +2 > ${TGT}',
		source='include/Optimizer/Optimizer.h', 
		target='README.md')


from waflib.Build import BuildContext, CleanContext, \
        InstallContext, UninstallContext

for x in 'debug release'.split():
        for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
                name = y.__name__.replace('Context','').lower()
                class tmp(y): 
                        cmd = name + '_' + x
                        variant = x
