#include <jaffarCommon/file.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/json.hpp>
#include "argparse/argparse.hpp"
#include "emuInstance.hpp"
#include "playbackInstance.hpp"

int main(int argc, char *argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("player", "1.0");

  program.add_argument("scriptFile")
    .help("Path to the test script file to run.")
    .required();

  program.add_argument("sequenceFile")
    .help("Path to the input sequence file (.sol) to reproduce.")
    .required();

  program.add_argument("--reproduce")
    .help("Plays the entire sequence without interruptions and exit at the end.")
    .default_value(false)
    .implicit_value(true);

  program.add_argument("--cycleType")
    .help("Specifies the emulation actions to be performed per each input. Possible values: 'Simple': performs only advance state, 'Rerecord': performs load/advance/save, and 'Full': performs load/advance/save/advance.")
    .default_value(std::string("Simple"));

  program.add_argument("--disableRender")
    .help("Do not render game window.")
    .default_value(false)
    .implicit_value(true);


  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting cycle type
  const auto cycleType = program.get<std::string>("--cycleType");
  bool cycleTypeRecognized = false;
  if (cycleType == "Simple") cycleTypeRecognized = true;
  if (cycleType == "Rerecord") cycleTypeRecognized = true;
  if (cycleTypeRecognized == false) JAFFAR_THROW_LOGIC("Unrecognized cycle type: %s\n", cycleType.c_str());
  
  // Getting test script file path
  const auto scriptFilePath = program.get<std::string>("scriptFile");

  // Loading script file
  std::string configJsRaw;
  if (jaffarCommon::file::loadStringFromFile(configJsRaw, scriptFilePath) == false) JAFFAR_THROW_LOGIC("Could not find/read script file: %s\n", scriptFilePath.c_str());

  // Parsing script
  const auto configJs = nlohmann::json::parse(configJsRaw);

  // Getting sequence file path
  std::string sequenceFilePath = program.get<std::string>("sequenceFile");

  // Getting reproduce flag
  bool isReproduce = program.get<bool>("--reproduce");

  // Getting reproduce flag
  bool disableRender = program.get<bool>("--disableRender");

  // Loading sequence file
  std::string inputSequence;
  auto status = jaffarCommon::file::loadStringFromFile(inputSequence, sequenceFilePath.c_str());
  if (status == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from sequence file: %s\n", sequenceFilePath.c_str());

  // Building sequence information
  const auto sequence = jaffarCommon::string::split(inputSequence, '\n');

  // Initializing terminal
  jaffarCommon::logger::initializeTerminal();

  // Printing provided parameters
  jaffarCommon::logger::log("[] Sequence File Path: '%s'\n", sequenceFilePath.c_str());
  jaffarCommon::logger::log("[] Sequence Length:    %lu\n", sequence.size());
  jaffarCommon::logger::log("[] Generating Sequence...\n");

  jaffarCommon::logger::refreshTerminal();

  // Creating emulator instance  
  auto e = jaffar::EmuInstance(configJs);

  // Initializing emulator instance
  e.initialize();

  // If rendering enabled, then initailize it now
  if (disableRender == false) e.enableRendering();

  // Creating playback instance
  auto p = PlaybackInstance(&e, sequence, cycleType);

  // Getting state size
  auto stateSize = e.getStateSize();

  // Flag to continue running playback
  bool continueRunning = true;

  // Variable for current step in view
  ssize_t sequenceLength = p.getSequenceLength();
  ssize_t currentStep = 0;

  // Flag to display frame information
  bool showFrameInfo = true;

  // Initializing video output
  if (disableRender == false) e.initializeVideoOutput();
  
  // Interactive section
  while (continueRunning)
  {
    // Updating display
    if (disableRender == false) p.renderFrame(currentStep);

    // Getting input
    const auto &inputString = p.getInputString(currentStep);

    // Getting state hash
    const auto hash = p.getStateHash(currentStep);

    // Getting state data
    const auto stateData = p.getStateData(currentStep);

    // Deserializing state
    jaffarCommon::deserializer::Contiguous d(stateData);
    e.deserializeState(d);

    // Printing data and commands
    if (showFrameInfo)
    {
      jaffarCommon::logger::clearTerminal();

      jaffarCommon::logger::log("[] ----------------------------------------------------------------\n");
      jaffarCommon::logger::log("[] Current Step #: %lu / %lu\n", currentStep + 1, sequenceLength);
      jaffarCommon::logger::log("[] Input:          %s\n", inputString.c_str());
      jaffarCommon::logger::log("[] State Hash:     0x%lX%lX\n", hash.first, hash.second);

      // Printing emulator information
      e.printInformation();

      // Only print commands if not in reproduce mode
      if (isReproduce == false) jaffarCommon::logger::log("[] Commands: n: -1 m: +1 | h: -10 | j: +10 | y: -100 | u: +100 | k: -1000 | i: +1000 | s: quicksave | p: play | q: quit\n");

      jaffarCommon::logger::refreshTerminal();
    }

    // Resetting show frame info flag
    showFrameInfo = true;

    // Get command
    auto command = jaffarCommon::logger::waitForKeyPress();

    // Advance/Rewind commands
    if (command == 'n') currentStep = currentStep - 1;
    if (command == 'm') currentStep = currentStep + 1;
    if (command == 'h') currentStep = currentStep - 10;
    if (command == 'j') currentStep = currentStep + 10;
    if (command == 'y') currentStep = currentStep - 100;
    if (command == 'u') currentStep = currentStep + 100;
    if (command == 'k') currentStep = currentStep - 1000;
    if (command == 'i') currentStep = currentStep + 1000;

    // Correct current step if requested more than possible
    if (currentStep < 0) currentStep = 0;
    if (currentStep >= sequenceLength) currentStep = sequenceLength - 1;

    // Quicksave creation command
    if (command == 's')
    {
      // Storing state file
      std::string saveFileName = "quicksave.state";

      std::string saveData;
      saveData.resize(stateSize);
      memcpy(saveData.data(), stateData, stateSize);
      if (jaffarCommon::file::saveStringToFile(saveData, saveFileName.c_str()) == false) JAFFAR_THROW_RUNTIME("[ERROR] Could not save state file: %s\n", saveFileName.c_str());
      jaffarCommon::logger::log("[] Saved state to %s\n", saveFileName.c_str());

      // Do no show frame info again after this action
      showFrameInfo = false;
    }

    // Start playback from current point
    if (command == 'p') isReproduce = true;

    // Start playback from current point
    if (command == 'q') continueRunning = false;
  }

  // Finalizing video output
  if (disableRender == false) e.finalizeVideoOutput();

  // If rendering enabled, then finalize it now
  if (disableRender == false) e.disableRendering();

  // Ending ncurses window
  jaffarCommon::logger::finalizeTerminal();
}
