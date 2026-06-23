# Getting core configurations

# Build base standalone, or in a subproject only when explicitly requested
# (buildBaseCore=true) -- e.g. to expose baseDependency for a rendering jaffar-player.
if meson.is_subproject() == false or get_option('buildBaseCore')
subdir('base')
endif

subdir('new')


