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

    #if the line is a C comment, skip it
    if line.strip().startswith('//') or line.strip().startswith('/*'): continue

    #if the line is a C preprocessor directive, skip it
    if line.strip().startswith('#'): continue

    # Check if the line contains a global variable
    continueRunning = True
    for var in globalVariables:
        for dataType in dataTypes:
            if continueRunning:
                pattern = f'\\b{dataType}\\s+{var}\\b'
                newLine = re.sub(pattern, f'__STORAGE_MODIFIER {dataType} {var}', line)

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

