# DSDA Core sources

DSDASrc =  [
]

DSDAIncludeDirs = [
  '.',
  '..',
]

DSDACompileArgs = [
  '-DHAVE_STDINT_H'
]

# DSDA Core Configuration

 baseDependency = declare_dependency(
  compile_args        : [  DSDACompileArgs],
  include_directories : include_directories(DSDAIncludeDirs),
  sources             : [ DSDASrc ],
  dependencies        : [  
                          dependency('sdl2',  required : true),
                        ]
 )
