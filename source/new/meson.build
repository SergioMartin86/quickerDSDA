# quickerDSDA Core sources

quickerDSDASrc =  [
]

quickerDSDAIncludeDirs = [
  '.',
  '..',
]

quickerDSDACompileArgs = [

]

# quickerDSDA Core Configuration

 newDependency = declare_dependency(
  compile_args        : [  quickerDSDACompileArgs ],
  include_directories : include_directories(quickerDSDAIncludeDirs),
  sources             : [ quickerDSDASrc ],
  dependencies        : [  
                          dependency('sdl2',  required : true),
                        ]
 )
