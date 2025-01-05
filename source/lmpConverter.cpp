#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/file.hpp>

#define _MAX_PLAYERS 4

struct playerInput_t
{
  int8_t forwardSpeed;
  int8_t strafingSpeed;
  int8_t turningSpeed;
  bool fire;
  bool action;
  uint8_t weapon;
};

int main(int argc, char *argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("tester", "1.0");

  program.add_argument("lmpFile")
    .help("Path to the input sequence file (.sol) to reproduce.")
    .required();

  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting test script file path
  const auto lmpFilePath = program.get<std::string>("lmpFile");

  // Loading lmp file
  std::string lmpRaw;
  if (jaffarCommon::file::loadStringFromFile(lmpRaw, lmpFilePath) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from lmp file: %s\n", lmpFilePath.c_str());

  // Casting input to byte array
  const uint8_t* input =  (const uint8_t*)lmpRaw.data();

  // File byte position
  size_t curPos = 0;

  // Reading: game version
  uint8_t version = input[curPos++];

  // Reading: skill
  uint8_t skill = input[curPos++];

  // Reading: episode
  uint8_t episode = input[curPos++];

  // Reading: map
  uint8_t map = input[curPos++];

  // Reading: multiplayer mode
  uint8_t multiplayerMode = input[curPos++];

  // Reading: monsters respawn
  uint8_t monstersRespawn = input[curPos++];

  // Reading: fast monsters
  uint8_t fastMonsters = input[curPos++];

  // Reading: no monsters
  uint8_t noMonsters = input[curPos++];

  // Reading: player point of view
  uint8_t playerPointOfView = input[curPos++];

  // Reading: player1 is present
  uint8_t player1Present = input[curPos++];

  // Reading: player2 is present
  uint8_t player2Present = input[curPos++];

  // Reading: player3 is present
  uint8_t player3Present = input[curPos++];

  // Reading: player4 is present
  uint8_t player4Present = input[curPos++];

  // Calculating player count
  uint8_t playerCount = 0;
  if (player1Present == 1) playerCount++;
  if (player2Present == 1) playerCount++;
  if (player3Present == 1) playerCount++;
  if (player4Present == 1) playerCount++;

  // Printing metadata finding to stderr

  fprintf(stderr, "Game Version:         %u\n", version);
  fprintf(stderr, "Skill Level:          %u\n", skill);
  fprintf(stderr, "Episode:              %u\n", episode);
  fprintf(stderr, "Map:                  %u\n", map);
  fprintf(stderr, "Multiplayer Mode:     %s\n", multiplayerMode == 1 ? "True" : "False");
  fprintf(stderr, "Monsters Respwan:     %s\n", monstersRespawn == 1 ? "True" : "False");
  fprintf(stderr, "Fast Monsters:        %s\n", fastMonsters == 1 ? "True" : "False");
  fprintf(stderr, "No Monsters:          %s\n", noMonsters == 1 ? "True" : "False");
  fprintf(stderr, "Player Point of View: %u\n", playerPointOfView);
  fprintf(stderr, "Player 1 Present:     %u\n", player1Present);
  fprintf(stderr, "Player 2 Present:     %u\n", player2Present);
  fprintf(stderr, "Player 3 Present:     %u\n", player3Present);
  fprintf(stderr, "Player 4 Present:     %u\n", player4Present);
  fprintf(stderr, "Player Count:         %u\n", playerCount);

  //// Reading inputs until reaching end token (0x80)
  std::vector<std::array<playerInput_t, _MAX_PLAYERS>> _inputVector;
  while (input[curPos] != 0x80)
  {
   std::array<playerInput_t, _MAX_PLAYERS> newInput;
   for (uint8_t playerId = 0; playerId < playerCount; playerId++)
   {
     newInput[playerId].forwardSpeed  = (int8_t)input[curPos++];
     newInput[playerId].strafingSpeed = (int8_t)input[curPos++];
     newInput[playerId].turningSpeed  = (int8_t)input[curPos++];

     uint8_t bits = input[curPos++];
     newInput[playerId].fire   = ((bits >> 0) & 0b00000001) > 0; 
     newInput[playerId].action = ((bits >> 1) & 0b00000001) > 0; 
     newInput[playerId].weapon = ((bits >> 2) & 0b00000111); 
   }

   // Pushing new tic input
   _inputVector.push_back(newInput);
  }

  // Getting tic count
  auto ticCount = _inputVector.size();

  // Printing tic count
  fprintf(stderr, "Tic Count:            %lu\n", ticCount);  

  // Printing inputs to stdout
  for (size_t ticIdx = 0; ticIdx < ticCount; ticIdx++)
  {
    for (uint8_t playerId = 0; playerId < playerCount; playerId++)
    {
      printf("|%3d,%3d,%3d,%s%s%1u",
       _inputVector[ticIdx][playerId].forwardSpeed,
       _inputVector[ticIdx][playerId].strafingSpeed,
       _inputVector[ticIdx][playerId].turningSpeed,
       _inputVector[ticIdx][playerId].fire ? "F" : ".",
       _inputVector[ticIdx][playerId].action ? "A" : ".",
       _inputVector[ticIdx][playerId].weapon
      );
    }

    // Line closure
    printf("|\n");
  }

  // If reached this point, everything ran ok
  return 0;
}
