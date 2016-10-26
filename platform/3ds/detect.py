
import os
import sys
import platform


def is_active():
	return True

def get_name():
        return "3ds"


def can_build():

	if (not os.environ.has_key("DEVKITPRO")):
		return False
	if (not os.environ.has_key("DEVKITARM")):
		return False
	if (not os.environ.has_key("CTRULIB")):
		return False
	if (os.name=="nt"):
		return False
	
	envstr='PKG_CONFIG_DIR= PKG_CONFIG_PATH= PKG_CONFIG_LIBDIR=${DEVKITPRO}/portlibs/3ds/lib/pkgconfig:${DEVKITPRO}/portlibs/armv6k/lib/pkgconfig'

	errorval=os.system("pkg-config --version > /dev/null")
	if (errorval):
		print("pkg-config not found... 3ds disabled.")
		return False
	
	for package in ['zlib', 'libpng']:
		errorval=os.system("{} pkg-config {} --modversion > /dev/null".format(envstr, package))
		if (errorval):
			print(package+" not found... 3ds disabled.")
			return False

	return True # 3DS enabled

def get_opts():

	return [
	('debug_release', 'Add debug symbols to release version','no'),
	]

def get_flags():

	return [
	('tools', 'no'),
	('squish', 'no'),
	('theora', 'no'),
	('vorbis', 'yes'),
	('speex', 'no'),
	('dds', 'no'),
	('pvr', 'no'),
	('etc1', 'no'),
	('builtin_zlib', 'no'),
	("openssl", "no"),
	('musepack', 'no'),
	]

def build_shader_gen(target, source, env, for_signature):
	return "picasso -o {} {}".format(target[0], source[0])

def build_shader_header(target, source, env):
	import os
	data = source[0].get_contents()
	data_str = ",".join([str(ord(x)) for x in data])
	name = os.path.basename(str(target[0]))[:-2]
	target[0].prepare()
	with open(str(target[0]), 'w') as f:
		f.write("/* Auto-generated from {} */\n".format(str(source[0])))
		f.write("static uint8_t shader_builtin_{}[] =\n{{{}}};".format(name, data_str))

def configure(env):
	
	env.disabled_modules = ['enet']

	env.Append( BUILDERS = { 'PICA' : env.Builder(generator = build_shader_gen, suffix = '.shbin', src_suffix = '.pica') } )
	env.Append( BUILDERS = { 'PICA_HEADER' : env.Builder(action = build_shader_header, suffix = '.h', src_suffix = '.shbin') } )
	
	env["bits"]="32"

	env.Append(CPPPATH=['#platform/3ds'])
	env["CC"]="arm-none-eabi-gcc"
	env["CXX"]="arm-none-eabi-g++"
	env["LD"]="arm-none-eabi-g++"
	env["AR"]="arm-none-eabi-ar"
	env["RANLIB"]="arm-none-eabi-ranlib"
	env["AS"]="arm-none-eabi-as"
	
	env.Append(CCFLAGS=['-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard', '-mtp=soft', '-mword-relocations'])
	env.Append(CCFLAGS=['-fomit-frame-pointer'])
	
	devkitpro_path = os.environ["DEVKITPRO"]
	ctrulib_path = os.environ["CTRULIB"]
	
	env.Append(CPPPATH=[devkitpro_path+"/portlibs/armv6k/include", devkitpro_path+"/portlibs/3ds/include"])
	env.Append(LIBPATH=[devkitpro_path+"/portlibs/armv6k/lib", devkitpro_path+"/portlibs/3ds/lib"])
	
	env.Append(LINKFLAGS=['-specs=3dsx.specs', '-g', '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard'])
	env.Append(CPPPATH=[ctrulib_path+"/include"])
	env.Append(LIBPATH=[ctrulib_path+"/lib"])
	env.Append(LIBS=["citro3d","ctru"])
	env.Append(LIBS=["png","z"])
	
	#if (env["use_llvm"]=="yes"):
		#if 'clang++' not in env['CXX']:
			#env["CC"]="clang"
			#env["CXX"]="clang++"
			#env["LD"]="clang++"
		#env.Append(CPPFLAGS=['-DTYPED_METHOD_BIND'])
		#env.extra_suffix=".llvm"

		#if (env["colored"]=="yes"):
			#if sys.stdout.isatty():
				#env.Append(CXXFLAGS=["-fcolor-diagnostics"])

	#if (env["use_sanitizer"]=="yes"):
		#env.Append(CXXFLAGS=['-fsanitize=address','-fno-omit-frame-pointer'])
		#env.Append(LINKFLAGS=['-fsanitize=address'])
		#env.extra_suffix+="s"

	#if (env["use_leak_sanitizer"]=="yes"):
		#env.Append(CXXFLAGS=['-fsanitize=address','-fno-omit-frame-pointer'])
		#env.Append(LINKFLAGS=['-fsanitize=address'])
		#env.extra_suffix+="s"


	#if (env["tools"]=="no"):
	#	#no tools suffix
	#	env['OBJSUFFIX'] = ".nt"+env['OBJSUFFIX']
	#	env['LIBSUFFIX'] = ".nt"+env['LIBSUFFIX']

	env.Append(CCFLAGS=['-D_3DS', '-DARM11', '-DNEED_LONG_INT', '-DLIBC_FILEIO_ENABLED'])

	if (env["target"]=="release"):
		if (env["debug_release"]=="yes"):
			env.Append(CCFLAGS=['-g2'])
		else:
			env.Append(CCFLAGS=['-O3'])
	elif (env["target"]=="release_debug"):
		env.Append(CCFLAGS=['-O2','-ffast-math','-DDEBUG_ENABLED'])
		if (env["debug_release"]=="yes"):
			env.Append(CCFLAGS=['-g2'])
	elif (env["target"]=="debug"):
		env.Append(CCFLAGS=['-g2', '-Wall','-DDEBUG_ENABLED','-DDEBUG_MEMORY_ENABLED'])

	#env.ParseConfig('pkg-config x11 --cflags --libs')
	#env.ParseConfig('pkg-config xrandr --cflags --libs')

	if (env["openssl"]=="yes"):
		env.ParseConfig('pkg-config openssl --cflags --libs')

	if (env["freetype"]=="yes"):
		env.ParseConfig('pkg-config freetype2 --cflags --libs')


	#env.Append(CPPFLAGS=['-DOPENGL_ENABLED'])

	#if (platform.system() == "Linux"):
		#env.Append(CPPFLAGS=["-DJOYDEV_ENABLED"])
	#if (env["udev"]=="yes"):
		## pkg-config returns 0 when the lib exists...
		#found_udev = not os.system("pkg-config --exists libudev")

		#if (found_udev):
			#print("Enabling udev support")
			#env.Append(CPPFLAGS=["-DUDEV_ENABLED"])
			#env.ParseConfig('pkg-config libudev --cflags --libs')
		#else:
			#print("libudev development libraries not found, disabling udev support")

	#if (env["pulseaudio"]=="yes"):
		#if not os.system("pkg-config --exists libpulse-simple"):
			#print("Enabling PulseAudio")
			#env.Append(CPPFLAGS=["-DPULSEAUDIO_ENABLED"])
			#env.ParseConfig('pkg-config --cflags --libs libpulse-simple')
		#else:
			#print("PulseAudio development libraries not found, disabling driver")

	#env.Append(CPPFLAGS=['-DX11_ENABLED','-DUNIX_ENABLED','-DGLES2_ENABLED','-DGLES_OVER_GL'])
	#env.Append(LIBS=['GL', 'pthread', 'z'])
	#if (platform.system() == "Linux"):
		#env.Append(LIBS='dl')
	#env.Append(CPPFLAGS=['-DMPC_FIXED_POINT'])
