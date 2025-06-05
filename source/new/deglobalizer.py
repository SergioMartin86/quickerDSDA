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
dataTypes = {'int', 'float', 'double', 'char', 'short', 'long', 'unsigned', 'signed', 'dboolean', 'fixed_t', 'gamestate_t', 'weaponinfo_t', 'player_t', 'mobj_t', 'sector_t', 'line_t', 'thinker_t', 'mapthing_t', 'angle_t', 'fixedangle_t'}

# Replace global variables in the source content
newSourceContent = sourceContent
for var in globalVariables:
    # Create a regex pattern to match the variable with its data type
    for dataType in dataTypes:
        pattern = f'\\b{dataType}\\s+\*?{var}\\b'
        #print("Pattern:", pattern)
        replacement = f'__STORAGE_MODIFIER {dataType} {var}'
        # Use re.sub to replace the variable with the new format
        newSourceContent = re.sub(pattern, replacement, newSourceContent)

# Compare the modified source content with the original
if sourceContent != newSourceContent:
    print("Changes made to the source file.")

#print(newSourceContent)

# Write the modified source content back to the file
with open(sourceFilePath, 'w') as sourceFile:
    sourceFile.write(newSourceContent)

