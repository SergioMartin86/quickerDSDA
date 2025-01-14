#pragma once

#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include "inputParser.hpp"
#include <SDL.h>
#include <d_player.h>

extern "C"
{
  int headlessMain(int argc, char **argv);
  void headlessRunSingleTick();
  void headlessUpdateSounds(void);
  void headlessClearTickCommand();
  void headlessSetTickCommand(int playerId, int forwardSpeed, int strafingSpeed, int turningSpeed, int fire, int action, int weapon, int altWeapon);

  // Video-related functions
  void headlessUpdateVideo(void);
  void* headlessGetVideoBuffer();
  int headlessGetVideoPitch();
  int headlessGetVideoWidth();
  int headlessGetVideoHeight();
  void headlessEnableRendering();
  void headlessDisableRendering();
  uint32_t* headlessGetPallette();

  void headlessSetSaveStatePointer(void* savePtr, int saveStateSize);
  size_t headlessGetEffectiveSaveSize();
  void dsda_ArchiveAll(void);
  void dsda_UnArchiveAll(void);
  void headlessGetMapName(char* outString);
}

// Players information
extern "C" int enableOutput;
extern "C" player_t players[MAX_MAXPLAYERS];
extern "C" int preventLevelExit;
extern "C" int preventGameEnd;
extern "C" int reachedLevelExit;
extern "C" int reachedGameEnd;
extern "C" int gamemap;
extern "C" int gametic;

namespace jaffar
{

#define SAVEGAMESIZE 0x100000

class EmuInstanceBase
{
  public:

  EmuInstanceBase(const nlohmann::json &config)
  {
    // Getting IWAD File Path
    _IWADFilePath = jaffarCommon::json::getString(config, "IWAD File Path");

    // Getting expected IWAD SHA1 hash
    _expectedIWADSHA1 = jaffarCommon::json::getString(config, "Expected IWAD SHA1");
 
    // Getting Doom parameters
    _skill  = jaffarCommon::json::getNumber<unsigned int>(config, "Skill Level");
    _episode  = jaffarCommon::json::getNumber<unsigned int>(config, "Episode");
    _map  = jaffarCommon::json::getNumber<unsigned int>(config, "Map");
    _compatibilityLevel  = jaffarCommon::json::getNumber<unsigned int>(config, "Compatibility Level");
    _fastMonsters = jaffarCommon::json::getBoolean(config, "Fast Monsters");
    _monstersRespawn = jaffarCommon::json::getBoolean(config, "Monsters Respawn");
    _noMonsters = jaffarCommon::json::getBoolean(config, "No Monsters");

    _preventLevelExit  = jaffarCommon::json::getBoolean(config, "Prevent Level Exit");
    _preventGameEnd  = jaffarCommon::json::getBoolean(config, "Prevent Game End");

    _player1Present = jaffarCommon::json::getBoolean(config, "Player 1 Present");
    _player2Present = jaffarCommon::json::getBoolean(config, "Player 2 Present");
    _player3Present = jaffarCommon::json::getBoolean(config, "Player 3 Present");
    _player4Present = jaffarCommon::json::getBoolean(config, "Player 4 Present");

    _playerCount = 0;
    if (_player1Present == true) _playerCount++;
    if (_player2Present == true) _playerCount++;
    if (_player3Present == true) _playerCount++;
    if (_player4Present == true) _playerCount++;

    // Parsing PWAD list
    auto pwadsJs = jaffarCommon::json::getArray<nlohmann::json>(config, "PWADS");
    for (auto entry : pwadsJs)
    {
      auto pwadFilePath = jaffarCommon::json::getString(entry, "File Path");
      auto pwadExpectedSHA1 = jaffarCommon::json::getString(entry, "Expected SHA1");
      _PWADFilePaths.push_back(pwadFilePath);
      _PWADExpectedSHA1s.push_back(pwadExpectedSHA1);
    }

    // Initializing input parser
    nlohmann::json inputParserConfig;
    inputParserConfig["Player Count"] = _playerCount;
    inputParserConfig["Controller Type"] = "Jaffar";
    _inputParser = std::make_unique<jaffar::InputParser>(inputParserConfig);
  }

  virtual ~EmuInstanceBase() 
  {
  }

  void initialize()
  {
    // Loading IWAD File
    std::string IWADFileData;
    if (jaffarCommon::file::loadStringFromFile(IWADFileData, _IWADFilePath) == false) JAFFAR_THROW_LOGIC("Could not IWAD file: %s\n", _IWADFilePath.c_str());

    // Calculating IWAD SHA1
    auto IWADSHA1 = jaffarCommon::hash::getSHA1String(IWADFileData);

    // Checking with the expected SHA1 hash
    if (IWADSHA1 != _expectedIWADSHA1) JAFFAR_THROW_LOGIC("Wrong IWAD SHA1. Found: '%s', Expected: '%s'\n", IWADSHA1.c_str(), _expectedIWADSHA1.c_str());

    // Loading PWAD Files
    for (size_t i = 0; i < _PWADFilePaths.size(); i++)
    {
      // Loading PWAD File
      std::string PWADFileData;
      if (jaffarCommon::file::loadStringFromFile(PWADFileData, _PWADFilePaths[i]) == false) JAFFAR_THROW_LOGIC("Could not PWAD file: %s\n", _PWADFilePaths[i].c_str());

      // Calculating IWAD SHA1
      auto PWADSHA1 = jaffarCommon::hash::getSHA1String(PWADFileData);

      // Checking with the expected SHA1 hash
      if (PWADSHA1 != _PWADExpectedSHA1s[i]) JAFFAR_THROW_LOGIC("Wrong PWAD SHA1. Found: '%s', Expected: '%s'\n", PWADSHA1.c_str(), _PWADExpectedSHA1s[i].c_str());
    }

    // Creating arguments
    int argc = 0;
    char** argv = (char**) malloc (sizeof(char*) * 512);
    
    // Specifying executable name
    char arg0[] = "dsda";
    argv[argc++] = arg0;

    // Specifying IWAD
    char arg1[] = "-iwad";
    argv[argc++] = arg1;
    char* iwadPath = (char*)((uint64_t)_IWADFilePath.c_str());
    argv[argc++] = iwadPath;

    // Eliminating restrictions to TAS inputs
    char arg2[] = "-tas";
    argv[argc++] = arg2;

    // Specifying skill level
    char arg3[] = "-skill";
    argv[argc++] = arg3;
    char argSkill[512];
    sprintf(argSkill, "%d", _skill);
    argv[argc++] = argSkill;

    // Specifying episode and map
    char arg4[] = "-warp";
    argv[argc++] = arg4;
    char argEpisode[512];
    if (_episode > 0)
    {
      sprintf(argEpisode, "%d", _episode);
      argv[argc++] = argEpisode;
    }
    char argMap[512];
    sprintf(argMap, "%d", _map);
    argv[argc++] = argMap;

    // Specifying comp level
    char arg5[] = "-complevel";
    argv[argc++] = arg5;
    char argCompatibilityLevel[512];
    sprintf(argCompatibilityLevel, "%d", _compatibilityLevel);
    argv[argc++] = argCompatibilityLevel;

    // Specifying fast monsters
    char arg6[] = "-fast";
    if (_fastMonsters) argv[argc++] = arg6;

    // Specifying monsters respawn
    char arg7[] = "-respawn";
    if (_monstersRespawn) argv[argc++] = arg7;

    // Specifying no monsters
    char arg8[] = "-nomonsters";
    if (_noMonsters) argv[argc++] = arg8;

    // Specifying PWAD Files
    char arg9[] = "-file";
    for (size_t i = 0; i < _PWADFilePaths.size(); i++)
    {
     argv[argc++] = arg9;
     char* pwadPath = (char*)((uint64_t)_PWADFilePaths[i].c_str());
     argv[argc++] = pwadPath;
    }

    // Initializing DSDA core
    headlessMain(argc, argv);

    // Getting video information
    _videoSource = (uint8_t*)headlessGetVideoBuffer();
    _videoWidth  = headlessGetVideoWidth();
    _videoHeight = headlessGetVideoHeight();
    _videoPitch  = headlessGetVideoPitch();

    // Calculating video buffer size
    int pixelBytes = 4; // RGB32
    _videoBufferSize = _videoWidth * _videoHeight * pixelBytes;

    // Allocating buffer
    _videoBuffer = (uint32_t*) malloc (_videoBufferSize);

    // Setting save state size
    _stateSize = SAVEGAMESIZE;

    // Setting level exit prevention flag
    if (_preventLevelExit == true) preventLevelExit = 1;

    // Setting level exit prevention flag
    if (_preventGameEnd == true) preventGameEnd = 1;

    // Enabling DSDA output, for debugging
    enableOutput = 1;
  }

  virtual void advanceState(const jaffar::input_t &input)
  {
    // Setting inputs
    headlessClearTickCommand();
    for (int i = 0; i < _playerCount; i++)
      headlessSetTickCommand
      (
        i,
        input[i].forwardSpeed,
        input[i].strafingSpeed,
        input[i].turningSpeed,
        input[i].fire ? 1 : 0,
        input[i].action ? 1 : 0,
        input[i].weapon,
        input[i].altWeapon ? 1 : 0
      );


    // Running a single tick
    headlessRunSingleTick();

    // If rendering is enabled, update vid now
    if(_renderingEnabled == true) 
    {
      headlessUpdateVideo();

      auto palette = headlessGetPallette();
      for (size_t i = 0; i < _videoWidth * _videoHeight; i++)
      {
       uint8_t* color = (uint8_t*)&palette[_videoSource[i]];
       uint8_t* video = (uint8_t*)&_videoBuffer[i];
       video[0] = color[2];
       video[1] = color[1];
       video[2] = color[0];
       video[3] = color[3];
      } 
    }

    if (reachedLevelExit == 1) jaffarCommon::logger::log("[] Level Exit detected on tic:   %d\n", gametic);
    if (reachedGameEnd   == 1) jaffarCommon::logger::log("[] Gane End detected on tic:   %d\n", gametic);
  }

  inline jaffarCommon::hash::hash_t getStateHash() const
  {
    MetroHash128 hash;

    // hash.Update(reachedLevelExit);
    // hash.Update(reachedGameEnd);
    hash.Update(gamemap);
    hash.Update(gametic);
    
    if (players[0].mo != nullptr)
    {
      hash.Update(players[0].mo->x);
      hash.Update(players[0].mo->y);
      hash.Update(players[0].mo->z);
      hash.Update(players[0].mo->angle);
      hash.Update(players[0].mo->momx);
      hash.Update(players[0].mo->momy);
      hash.Update(players[0].mo->momz);
      hash.Update(players[0].mo->health);
    }

    jaffarCommon::hash::hash_t result;
    hash.Finalize(reinterpret_cast<uint8_t *>(&result));
    return result;
  }

  int getMapNumber () const { return gamemap; }
  bool isLevelExit () const { return reachedLevelExit == 1; }
  bool isGameEnd () const { return reachedGameEnd == 1; }

  void printInformation()
  {
    char mapName[512];
    headlessGetMapName(mapName);
    jaffarCommon::logger::log("[] Map:        %s\n", mapName);
    jaffarCommon::logger::log("[] Game Tic:   %d\n", gametic);
    jaffarCommon::logger::log("[] Level Exit: %s\n", reachedLevelExit == 1 ? "Yes" : "No");
    jaffarCommon::logger::log("[] Game End:   %s\n", reachedGameEnd   == 1 ? "Yes" : "No");
 
    if (players[0].mo != nullptr)
    {
      jaffarCommon::logger::log("[] Player 1 Coordinates:    (%d, %d, %d)\n", players[0].mo->x, players[0].mo->y, players[0].mo->z);
      jaffarCommon::logger::log("[] Player 1 Angle:           %u\n", players[0].mo->angle);
      jaffarCommon::logger::log("[] Player 1 Momenta:        (%d, %d, %d)\n", players[0].mo->momx, players[0].mo->momy, players[0].mo->momz);
      jaffarCommon::logger::log("[] Player 1 Health:          %d\n", players[0].mo->health);
    }
  }

  void initializeVideoOutput()
  {
    SDL_Init(SDL_INIT_VIDEO);
    _renderWindow = SDL_CreateWindow("QuickerDSDA",  SDL_WINDOWPOS_UNDEFINED,  SDL_WINDOWPOS_UNDEFINED, _videoWidth, _videoHeight, 0);
    _renderer = SDL_CreateRenderer(_renderWindow, -1, SDL_RENDERER_ACCELERATED);
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, _videoWidth, _videoHeight);
  }

  void finalizeVideoOutput()
  {
    SDL_DestroyTexture(_texture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_renderWindow);
    SDL_Quit();
  }

  void enableRendering()
  {
    headlessEnableRendering();
    _renderingEnabled = true;
  }

  void disableRendering()
  {
    headlessDisableRendering();
    _renderingEnabled = false;
  }

  void updateRenderer()
  {
    SDL_Rect srcRect  = { 0, 0, _videoWidth, _videoHeight };
    SDL_Rect destRect = { 0, 0, _videoWidth, _videoHeight };

    void *pixels = nullptr;
    int pitch = _videoPitch;
    if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) < 0) return;
    memcpy(pixels, _videoBuffer, _videoBufferSize);
    SDL_UnlockTexture(_texture);
    SDL_RenderClear(_renderer);
    SDL_RenderCopy(_renderer, _texture, &srcRect, &destRect);
    SDL_RenderPresent(_renderer);
  }

  inline size_t getStateSize() const 
  {
    return _stateSize;
  }

  inline jaffar::InputParser *getInputParser() const { return _inputParser.get(); }
  
  void serializeState(jaffarCommon::serializer::Base& s) const
  {
    headlessSetSaveStatePointer(s.getOutputDataBuffer(), _stateSize);
    dsda_ArchiveAll();
  }

  void deserializeState(jaffarCommon::deserializer::Base& d) 
  {
    headlessSetSaveStatePointer((void*)((uint64_t)d.getInputDataBuffer()), _stateSize);
    dsda_UnArchiveAll();
  }

  size_t getVideoBufferSize() const { return _videoBufferSize; }
  uint8_t* getVideoBufferPtr() const { return (uint8_t*)_videoBuffer; }
  size_t getEffectiveSaveStateSize() const { return headlessGetEffectiveSaveSize(); }

  // Virtual functions

  virtual void doSoftReset() = 0;
  virtual void doHardReset() = 0;
  virtual std::string getCoreName() const = 0;

  protected:


  virtual void setWorkRamSerializationSizeImpl(const size_t size) {};
  virtual void enableStateBlockImpl(const std::string& block) {};
  virtual void disableStateBlockImpl(const std::string& block) {};

  // State size
  size_t _stateSize;

  private:

  std::string _IWADFilePath;
  std::string _expectedIWADSHA1;

  std::vector<std::string> _PWADFilePaths;
  std::vector<std::string> _PWADExpectedSHA1s;

  unsigned int _skill; 
  unsigned int _episode;
  unsigned int _map;
  unsigned int _compatibilityLevel;
  bool _fastMonsters;
  bool _monstersRespawn;
  bool _noMonsters;
  bool _preventLevelExit;
  bool _preventGameEnd;
  bool _player1Present;  
  bool _player2Present;  
  bool _player3Present;  
  bool _player4Present;  
  uint8_t _playerCount;

  std::unique_ptr<jaffar::InputParser> _inputParser;
  static uint32_t InputGetter(void* inputValue) { return *(uint32_t*)inputValue; }

  // Rendering stuff
  int _videoWidth;
  int _videoHeight;
  int _videoPitch;
  SDL_Window* _renderWindow;
  SDL_Renderer* _renderer;
  SDL_Texture* _texture;
  uint8_t* _videoSource;
  uint32_t* _videoBuffer;
  size_t _videoBufferSize;
  bool _renderingEnabled = false;
};

} // namespace jaffar