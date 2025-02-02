#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/timing.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/file.hpp>
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

  program.add_argument("--cycleType")
    .help("Specifies the emulation actions to be performed per each input. Possible values: 'Simple': performs only advance state, 'Rerecord': performs load/advance/save, and 'Full': performs load/advance/save/advance.")
    .default_value(std::string("Simple"));

  program.add_argument("--hashOutputFile")
    .help("Path to write the hash output to.")
    .default_value(std::string(""));

  program.add_argument("--rerecordDepth")
    .help("How many pre-advances to do when using a rerecord cycle.")
    .default_value(std::string("1"));

  program.add_argument("--warmup")
  .help("Warms up the CPU before running for reduced variation in performance results")
  .default_value(false)
  .implicit_value(true);

  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting test script file path
  const auto scriptFilePath = program.get<std::string>("scriptFile");

  // Getting path where to save the hash output (if any)
  const auto hashOutputFile = program.get<std::string>("--hashOutputFile");

  // Getting cycle type
  const auto cycleType = program.get<std::string>("--cycleType");

  // Parsing re-record depth
  const auto rerecordDepth = std::stoi(program.get<std::string>("--rerecordDepth"));

  bool cycleTypeRecognized = false;
  if (cycleType == "Simple") cycleTypeRecognized = true;
  if (cycleType == "Rerecord") cycleTypeRecognized = true;
  if (cycleTypeRecognized == false) JAFFAR_THROW_LOGIC("Unrecognized cycle type: %s\n", cycleType.c_str());

  // Getting warmup setting
  const auto useWarmUp = program.get<bool>("--warmup");

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

  // Creating emulator instance
  auto e = jaffar::EmuInstance(configJs);

  // Initializing emulator instance
  e.initialize();
  
  // Disable rendering
  e.disableRendering();

  // Getting full state size
  const auto stateSize = e.getStateSize();

  // Loading sequence file
  std::string sequenceRaw;
  if (jaffarCommon::file::loadStringFromFile(sequenceRaw, sequenceFilePath) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from input sequence file: %s\n", sequenceFilePath.c_str());

  // Building sequence information
  const auto sequence = jaffarCommon::string::split(sequenceRaw, '\n');

  // Getting sequence lenght
  const auto sequenceLength = sequence.size();

  // Getting input parser from the emulator
  const auto inputParser = e.getInputParser();

  // Getting decoded emulator input for each entry in the sequence
  std::vector<jaffar::input_t> decodedSequence;
  for (const auto &inputString : sequence) decodedSequence.push_back(inputParser->parseInputString(inputString));

  // Creating RNG generator
  std::random_device seed;
  std::mt19937 rng{seed()}; // seed the generator

  // Getting emulation core name
  std::string emulationCoreName = e.getCoreName();

  // Printing test information
  printf("[] -----------------------------------------\n");
  printf("[] Running Script:                         '%s'\n", scriptFilePath.c_str());
  printf("[] Cycle Type:                             '%s'\n", cycleType.c_str());
  printf("[] Emulation Core:                         '%s'\n", emulationCoreName.c_str());
  printf("[] Sequence File:                          '%s'\n", sequenceFilePath.c_str());
  printf("[] Sequence Length:                        %lu\n", sequenceLength);

  if (cycleType == "Rerecord")
  printf("[] State Size:                             %lu bytes\n", stateSize);
  
  // If warmup is enabled, run it now. This helps in reducing variation in performance results due to CPU throttling
  if (useWarmUp)
  {
    printf("[] ********** Warming Up **********\n");

    auto tw = jaffarCommon::timing::now();
    double waitedTime = 0.0;
    #pragma omp parallel
    while(waitedTime < 2.0) waitedTime = jaffarCommon::timing::timeDeltaSeconds(jaffarCommon::timing::now(), tw);
  }

  printf("[] ********** Running Test **********\n");

  fflush(stdout);

  // Serializing initial state
  auto currentState = (uint8_t *)malloc(stateSize);
  {
    jaffarCommon::serializer::Contiguous cs(currentState);
    e.serializeState(cs);
  }

  // Check whether to perform each action
  bool doPreAdvance = cycleType == "Rerecord";
  bool doDeserialize = cycleType == "Rerecord";
  bool doSerialize = cycleType == "Rerecord";

  // Actually running the sequence
  auto t0 = std::chrono::high_resolution_clock::now();
  for (const auto &input : decodedSequence)
  {
    if (doPreAdvance == true) 
    {
      for (size_t i = 0; i < rerecordDepth; i++) e.advanceState(generateRandomInput(rng));
    }
    
    if (doDeserialize == true)
    {
      jaffarCommon::deserializer::Contiguous d(currentState, stateSize);
      e.deserializeState(d);
    } 
    
    e.advanceState(input);

    if (doSerialize == true)
    {
      auto s = jaffarCommon::serializer::Contiguous(currentState, stateSize);
      e.serializeState(s);
    } 
  }
  auto tf = std::chrono::high_resolution_clock::now();

  // Calculating running time
  auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(tf - t0).count();
  double elapsedTimeSeconds = (double)dt * 1.0e-9;

  // Calculating final state hash
  auto result = e.getStateHash();

  // Creating hash string
  char hashStringBuffer[256];
  sprintf(hashStringBuffer, "0x%lX%lX", result.first, result.second);

  // Final game information
  e.printInformation();

  // Printing time information
  printf("[] Elapsed time:                           %3.3fs\n", (double)dt * 1.0e-9);
  printf("[] Performance:                            %.3f inputs / s\n", (double)sequenceLength / elapsedTimeSeconds);
  printf("[] Final State Hash:                       %s\n", hashStringBuffer);

  if (cycleType == "Rerecord")
  printf("[] Effective Save State Size:              %lu bytes\n", e.getEffectiveSaveStateSize());

  // Checking expected consitions
  auto mapNumber = e.getMapNumber ();
  auto isLevelExit = e.isLevelExit ();
  auto isGameEnd = e.isGameEnd ();

  if (mapNumber != expectedMapNumber) { printf("[] Test Failed: Map Number (%d) different from expected one (%d)\n", mapNumber, expectedMapNumber); return -1; }

  // These tests don't work correctly for rerecording
  if (cycleType != "Rerecord")
  {
    if (isLevelExit != expectedIsLevelExit) { printf("[] Test Failed: Failed to reach level exit on the last tic\n"); return -1; }
    if (isGameEnd != expectedIsGameEnd) { printf("[] Test Failed: Failed to reach game end on the last tic\n"); return -1; }
  }

 
  // If saving hash, do it now
  if (hashOutputFile != "") jaffarCommon::file::saveStringToFile(std::string(hashStringBuffer), hashOutputFile.c_str());

  // If reached this point, everything ran ok
  return 0;
}
