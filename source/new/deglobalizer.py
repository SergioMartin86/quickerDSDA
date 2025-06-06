# import modules used here -- sys is a very standard one
import sys
import re

globalsFilePath = sys.argv[1]
sourceFilePath = sys.argv[2]

print("Globals File:", globalsFilePath)
print("Source File:", sourceFilePath)

# Read the globals file
with open(globalsFilePath, 'r') as globalsFile:
    globalsContent = globalsFile.read()

# Read the source file
with open(sourceFilePath, 'r') as sourceFile:
    sourceContent = sourceFile.read()

# Make a set of all global variables (3rd word in each line)
globalVariables = set()
for line in globalsContent.splitlines():
    parts = line.split()
    if len(parts) >= 3:
        globalVariables.add(parts[2])  

# Print global variables for debugging
# print("Global Variables:", globalVariables)

# Set of possible C variable data types
dataTypes = {'int', 'float', 'double', 'char', 'const char','short', 'long', 'unsigned', 'signed',
              'dboolean', 'fixed_t', 'gamestate_t', 'weaponinfo_t', 'player_t', 'mobj_t',
                'sector_t', 'line_t', 'thinker_t', 'mapthing_t', 'angle_t', 'fixedangle_t',
                  'acsInfo_t', 'rcolumn_t', 'rpatch_t', 'rpost_t', 'count_t',
                   'pclass_t', 'ticcmd_t', 'puser_t', 'pipeinfo_t', 'elevator_t'
                    'ceiling_t', 'vldoor_t', 'plat_t', 'floormove_t', 'fixed2_t'
                     'visplane_t', 'anim_t', 'uint64_t', 'button_t', 'switchlist_t'
                      'lumpinfo_t', 'wadfile_info_t', 'state_t', 'glob_t', 
                       'dsda_arg_t', 'map_stats_t', 'setup_menu_t', 'menu_t',
                        'menuitem_t', 'trailpoint_t', 'mpoint_t', 'mline_t',
                         'array_t', 'am_frame_t', 'map_trail_mode_t', 'vissprite_t',
                           'weaponinfo_t', 'pspdef_t', 'spriteframe_t', 'spritedef_t',
                            'draw_column_vars_t', 'edgeslope_t', 'ammotype_t', 'thing_id_search_t',
                            'artitype_t', 'pusher_t', 'damage_t', 'degenmobj_t', 'animdef_t',
                            'planeWaggle_t', 'map_nice_thing_t', 'texpatch_t', 'map_point_t',
                            'pillar_t', 'channel_t', 'sfxinfo_t', 'int64_t', 'mappatch_t',
                            'maptexture_t', 'wi_anim_t', 'stateenum_t', 'ssline_t', 'subsector_t',
                            'seg_t', 'vertex_t', 'node_t', 'side_t', 'raven_mobjinfo_t', 'inventory_t',
                            'musicinfo_t', 'patchnum_t', 'vbo_xyz_uv_t', 'gl_strip_coords_t', 'color_rgb_t',
                            'stretch_param_t', 'map_loader_t', 'blockmap_t' }



# Now iterate sourceContent line by line
isInStruct = False
newSourceContent = sourceContent
for line in newSourceContent.splitlines():
    print("Processing line:", line.strip())
    
    if ('}' in line): isInStruct = False
    if ('typedef struct' in line): isInStruct = True
    if (isInStruct): continue  # Skip lines inside structs
    #if the line is empty, skip it
    if not line.strip(): continue
    if ('\\' in line): continue
    #if ('const' in line): continue
    #if ('void' in line): continue
    if ('(' in line): continue
    if ('__STORAGE_MODIFIER' in line): continue
    if not any(dataType in line for dataType in dataTypes): continue
    if not any(gvar in line for gvar in globalVariables): continue

    #if the line is a C comment, skip it
    if line.strip().startswith('//') or line.strip().startswith('/*'): continue

    #if the line is a C preprocessor directive, skip it
    if line.strip().startswith('#'): continue

    # Check if the line contains a global variable
    continueRunning = True
    for var in globalVariables:
        for dataType in dataTypes:
            if continueRunning:

                # For non-pointers
                #pattern = f'\\b{dataType}\\s+{var}\\b'
                #newLine = re.sub(pattern, f'__STORAGE_MODIFIER {dataType} {var}', line)

                #For pointers
                pattern = f'\\b{dataType}\\s*\\*\\s*{var}\\b'
                newLine = re.sub(pattern, f'__STORAGE_MODIFIER {dataType} *{var}', line)

                # If the line was modified, update the source content
                if newLine != line:
                    print(f"Replacing '{line}' with '{newLine}")
                    newSourceContent = newSourceContent.replace(line, newLine)
                    continueRunning = False

# Compare the modified source content with the original
if sourceContent != newSourceContent:
    print("Changes made to the source file.")

#print(newSourceContent)

# Write the modified source content back to the file
with open(sourceFilePath, 'w') as sourceFile:
    sourceFile.write(newSourceContent)

