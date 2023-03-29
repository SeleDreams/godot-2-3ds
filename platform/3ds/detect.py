
import os
import sys
import platform


def is_active():
    return True


def get_name():
    return "3ds"


def can_build():
    if (not os.getenv("DEVKITPRO")):
        print("DEVKITPRO not found in environment! 3DS target disabled!")
        return False
    return True  # 3DS enabled


def get_opts():
    return [
        ('debug_release', 'Add debug symbols to release version', 'no'),
    ]


def get_flags():

    return [
        ('tools', 'no'),
        ('module_squish_enabled', 'no'),
        ('module_theora_enabled', 'no'),
        ('module_vorbis_enabled', 'yes'),
        ('module_speex_enabled', 'no'),
        ('module_dds_enabled', 'no'),
        ('module_pvr_enabled', 'no'),
        ('module_etc1_enabled', 'no'),
        ('builtin_zlib', 'yes'),
        ('builtin_freetype','yes'),
        ("module_openssl_enabled", "no"),
        ('module_musepack_enabled', 'no')
    ]


def build_shader_gen(target, source, env, for_signature):
    return "picasso -o {} {}".format(target[0], source[0])


def build_shader_header(target, source, env):
    import os
    data = source[0].get_contents()
    data_str = ",".join([str(x) for x in data])
    name = os.path.basename(str(target[0]))[:-2]
    target[0].prepare()
    with open(str(target[0]), 'w') as f:
        f.write("/* Auto-generated from {} */\n".format(str(source[0])))
        f.write("static uint8_t shader_builtin_{}[] =\n{{{}}};".format(
            name, data_str))


def configure(env):
    env.use_windows_spawn_fix()
    env.disabled_modules = ['enet']

    env.Append(BUILDERS={'PICA': env.Builder(
        generator=build_shader_gen, suffix='.shbin', src_suffix='.pica')})
    env.Append(BUILDERS={'PICA_HEADER': env.Builder(
        action=build_shader_header, suffix='.h', src_suffix='.shbin')})

    env["bits"] = "32"
    devkitpro_path = os.environ["DEVKITPRO"]
    devkitarm_path = devkitpro_path + "/devkitARM"
    ctrulib_path = devkitpro_path + "/libctru"
    env.Append(CPPPATH=['#platform/3ds'])
    env["CC"] = devkitarm_path + "/bin/arm-none-eabi-gcc"
    env["CXX"] = devkitarm_path + "/bin/arm-none-eabi-g++"
    env["LD"] = devkitarm_path + "/bin/arm-none-eabi-g++"
    env["AR"] = devkitarm_path + "/bin/arm-none-eabi-ar"
    env["RANLIB"] = devkitarm_path + "/bin/arm-none-eabi-ranlib"
    env["AS"] = devkitarm_path + "/bin/arm-none-eabi-as"

    arch = ['-march=armv6k', '-mtune=mpcore','-mfloat-abi=hard','-mtp=soft' ]
    env.Append(CCFLAGS=['-g','-Wall','-mword-relocations','-ffunction-sections', '-fno-rtti', '-fno-exceptions', '-std=gnu++11'] + arch)
    env.Append(CCFLAGS=['-D_3DS', '-DARM11','-DNEED_LONG_INT', '-DLIBC_FILEIO_ENABLED','-DNO_SAFE_CAST'])
    env.Append(CPPPATH=[devkitpro_path+"/portlibs/armv6k/include", devkitpro_path +
               "/portlibs/3ds/include", ctrulib_path + "/include", devkitarm_path + "/arm-none-eabi/include"])
    env.Append(LIBPATH=[devkitpro_path+"/portlibs/armv6k/lib", devkitpro_path +
               "/portlibs/3ds/lib", ctrulib_path + "/lib", devkitarm_path + "/arm-none-eabi/lib/armv6k/fpu"])

    env.Append(LINKFLAGS=['-specs=3dsx.specs', '-g'] + arch)
    env.Append(LIBS=["citro3d", "ctru","bz2"])
    env.Append(LIBS=["png", "z"])

   

    if (env["target"] == "release"):
        if (env["debug_release"] == "yes"):
            env.Append(CCFLAGS=['-g2'])
        else:
            env.Append(CCFLAGS=['-O3'])
    elif (env["target"] == "release_debug"):
        env.Append(CCFLAGS=['-O2', '-ffast-math', '-DDEBUG_ENABLED'])
        if (env["debug_release"] == "yes"):
            env.Append(CCFLAGS=['-g2'])
    elif (env["target"] == "debug"):
        env.Append(CCFLAGS=['-O3','-g2','-Wall',
                   '-DDEBUG_ENABLED', '-DDEBUG_MEMORY_ENABLED'])
