#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/timing.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/parallel.hpp>
#include "emuInstance.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <vector>
#include <string>

jaffar::input_t generateRandomInput(std::mt19937& rng)
{
  jaffar::input_t randomInput;

  std::uniform_int_distribution<> forwardSpeedDist{-50, 50};
  randomInput[0].forwardSpeed = forwardSpeedDist(rng);

  std::uniform_int_distribution<> strafingSpeedDist{-50, 50};
  randomInput[0].strafingSpeed = strafingSpeedDist(rng);

  std::uniform_int_distribution<> turningSpeedDist{-120, 120};
  randomInput[0].turningSpeed = turningSpeedDist(rng);

  std::uniform_int_distribution<> fireDist{0, 1};
  randomInput[0].fire = fireDist(rng) == 1;

  std::uniform_int_distribution<> actionDist{0, 1};
  randomInput[0].action = actionDist(rng) == 1;

  std::uniform_int_distribution<> weaponDist{0, 7};
  randomInput[0].weapon = weaponDist(rng);

  return randomInput;
}

int main(int argc, char *argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("tester", "1.0");

  program.add_argument("scriptFile")
    .help("Path to the test script file to run.")
    .required();

  program.add_argument("sequenceFile")
    .help("Path to the input sequence file (.sol) to reproduce.")
    .required();

  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting test script file path
  const auto scriptFilePath = program.get<std::string>("scriptFile");

  // Loading script file
  std::string configJsRaw;
  if (jaffarCommon::file::loadStringFromFile(configJsRaw, scriptFilePath) == false) JAFFAR_THROW_LOGIC("Could not find/read script file: %s\n", scriptFilePath.c_str());

  // Parsing script
  const auto configJs = nlohmann::json::parse(configJsRaw);

  // Getting expected result parameters
  auto expectedResult = jaffarCommon::json::getObject(configJs, "Expected Result");
  auto expectedMapNumber   = jaffarCommon::json::getNumber<int>(expectedResult, "Map Number");
  auto expectedIsLevelExit = jaffarCommon::json::getBoolean(expectedResult, "Is Level Exit");
  auto expectedIsGameEnd   = jaffarCommon::json::getBoolean(expectedResult, "Is Game End");

  // Getting sequence file path
  std::string sequenceFilePath = program.get<std::string>("sequenceFile");

  // Loading sequence file
  std::string sequenceRaw;
  if (jaffarCommon::file::loadStringFromFile(sequenceRaw, sequenceFilePath) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from input sequence file: %s\n", sequenceFilePath.c_str());

  // Building sequence information
  const auto sequence = jaffarCommon::string::split(sequenceRaw, '\n');

  // Getting sequence lenght
  const auto sequenceLength = sequence.size();

  // Mutex for common checks
  std::mutex mutex;

  // Hash verification string, set by the first to finish, all the others need to coincide
  std::string verificationHash = "";

  // Flag for successful execution
  bool isSuccess = true;

  // Printing test information
  printf("[] -----------------------------------------\n");
  printf("[] Running Script:                         '%s'\n", scriptFilePath.c_str());
  printf("[] Sequence File:                          '%s'\n", sequenceFilePath.c_str());
  printf("[] Sequence Length:                        %lu\n", sequenceLength);
  printf("[] ********** Running Test **********\n");
  fflush(stdout);


  // Getting full state size
  const auto stateSize = SAVEGAMESIZE;
  
  // Buffer for state data
  auto stateData = (uint8_t *)malloc(stateSize);

  JAFFAR_PARALLEL
  {
    // Getting thread id
    int threadId = jaffarCommon::parallel::getThreadId();

    // Creating emulator instance
    auto e = jaffar::EmuInstance(configJs);

    // Initializing emulator instance
    e.initialize();
      
    // Disable rendering
    e.disableRendering();

    // Getting input parser from the emulator
    const auto inputParser = e.getInputParser();

    // Getting decoded emulator input for each entry in the sequence
    std::vector<jaffar::input_t> decodedSequence;
    for (const auto &inputString : sequence) decodedSequence.push_back(inputParser->parseInputString(inputString));

    for (const auto &input : decodedSequence) 
    {
      // Master saves state
      if (threadId == 0)
      {
        // Actually running the sequence
        e.advanceState(input);

        // Saving state
        jaffarCommon::serializer::Contiguous cs(stateData);
        e.serializeState(cs);
      }

      // Barrier
      JAFFAR_BARRIER;
    
      if (threadId > 0)
      {
        // JAFFAR_CRITICAL
        {
          // Secondary loads state
          jaffarCommon::deserializer::Contiguous d(stateData, stateSize);

          e.deserializeState(d);
        }
      }
    }

    // Calculating final state hash
    auto result = e.getStateHash();

    // Creating hash string
    char hashStringBuffer[256];
    sprintf(hashStringBuffer, "0x%lX%lX", result.first, result.second);
    std::string hashString = std::string(hashStringBuffer);

    // Hash verification
    mutex.lock();
    if (verificationHash == "") verificationHash = hashString;
    else if (hashString != verificationHash) { printf("[] Test Failed: Diverging Hashes (%s vs %s)\n", hashString.c_str(), verificationHash.c_str()); isSuccess = false; }
    mutex.unlock();

    // Checking expected consitions
    auto mapNumber = e.getMapNumber ();
    auto isLevelExit = e.isLevelExit ();
    auto isGameEnd = e.isGameEnd ();

    if (mapNumber != expectedMapNumber) { printf("[] Test Failed: Map Number (%d) different from expected one (%d)\n", mapNumber, expectedMapNumber); isSuccess = false; }
    if (isLevelExit != expectedIsLevelExit) { printf("[] Test Failed: Failed to reach level exit on the last tic\n"); isSuccess = false; }
    if (isGameEnd != expectedIsGameEnd) { printf("[] Test Failed: Failed to reach game end on the last tic\n"); isSuccess = false; }
  }
 
  // If failed, return now
  if (isSuccess == false) return -1;

  // If reached this point, everything ran ok
  printf("[] Successful Execution.\n");
  printf("[] Final State Hash:                       %s\n", verificationHash.c_str());
  return 0;
}
