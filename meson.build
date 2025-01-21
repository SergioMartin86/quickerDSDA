project('quickerDSDA','c','cpp',
  version: '1.0.0',
  default_options : ['cpp_std=c++20', 'default_library=shared', 'buildtype=release'],
  subproject_dir : 'extern'
)

# Loading dependencies
subdir('source')

# Do not build any targets if this is a subproject
if meson.is_subproject() == false

# Common application flags
commonCompileArgs = [ '-Wall', '-Wfatal-errors' ]

# Grabbing jaffarCommon dependency

jaffarCommonSubproject = subproject('jaffarCommon')
jaffarCommonDependency = jaffarCommonSubproject.get_variable('jaffarCommonDependency')

# Building playback tool

if get_option('buildPlayer') == true

  basePlayer = executable('basePlayer',
    'source/player.cpp',
    cpp_args            : [ commonCompileArgs, '-DNCURSES' ],
    dependencies        : [ baseDependency,
                            jaffarCommonDependency,
                            dependency('sdl2',  required : true)
                          ],
    include_directories : include_directories(['source']),
    link_args           : [ '-lncurses' ],
  )

endif

# Building parallel tester tool

pTester = executable('pTester',
  'source/parallel.cpp',
  cpp_args            : [ commonCompileArgs ], 
  dependencies        : [ newDependency, jaffarCommonDependency, dependency('sdl2',  required : true) ],
)


# Building tester tool

newTester = executable('newTester',
  'source/tester.cpp',
  cpp_args            : [ commonCompileArgs ], 
  dependencies        : [ newDependency, jaffarCommonDependency, dependency('sdl2',  required : true) ],
)

# Building tester tool for the original emulator

baseTester = executable('baseTester',
  'source/tester.cpp',
  cpp_args            : [ commonCompileArgs ],
  dependencies        : [ baseDependency,
                          jaffarCommonDependency,
                          dependency('sdl2',  required : true) ],
)

# Building converter tool

lmpConverter = executable('lmpConverter',
  'source/lmpConverter.cpp',
  cpp_args            : [ commonCompileArgs ],
  dependencies        : [ baseDependency, jaffarCommonDependency ],
)

# Building tests
subdir('tests')

endif # If not subproject
