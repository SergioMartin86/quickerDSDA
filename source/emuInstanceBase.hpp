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

extern "C"
{
  int headlessMain(int argc, char **argv);
  void headlessRunSingleTick();
  void headlessUpdateSounds(void);
  void headlessClearTickCommand();
  void headlessSetTickCommand(int playerId, int forwardSpeed, int strafingSpeed, int turningSpeed, int fire, int action, int weapon);

  // Video-related functions
  void headlessUpdateVideo(void);
  void* headlessGetVideoBuffer();
  int headlessGetVideoPitch();
  int headlessGetVideoWidth();
  int headlessGetVideoHeight();
  SDL_Surface* headlessGetVideoSurface();
  void headlessEnableRendering();
  void headlessDisableRendering();
}

namespace jaffar
{

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
  }

  virtual ~EmuInstanceBase() 
  {
  }

  virtual void advanceState(const jaffar::input_t &input)
  {
    // Setting inputs
    headlessClearTickCommand();
    //headlessSetTickCommand(int playerId, int forwardSpeed, int strafingSpeed, int turningSpeed, int fire, int action, int weapon = -1);
    headlessSetTickCommand(0, 50, 0, 0, 0, 0, 0);


    headlessRunSingleTick();

    // If rendering is enabled, update vid
    if(_renderingEnabled == true) headlessUpdateVideo();
  }

  inline jaffarCommon::hash::hash_t getStateHash() const
  {
    MetroHash128 hash;
    
    jaffarCommon::hash::hash_t result;
    hash.Finalize(reinterpret_cast<uint8_t *>(&result));
    return result;
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
    sprintf(argEpisode, "%d", _episode);
    argv[argc++] = argEpisode;
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

    // Initializing DSDA core
    headlessMain(argc, argv);

    // Getting video information
    _baseSurface = headlessGetVideoSurface();
    _videoBuffer = _baseSurface->pixels;
    _videoWidth = _baseSurface->w;
    _videoHeight = _baseSurface->h;
    _videoPitch = _baseSurface->pitch;

    // Calculating video buffer size
    int pixelBytes = 4; // RGB32
    _videoBufferSize = _videoWidth * _videoHeight * pixelBytes;
  }

  void initializeVideoOutput()
  {
    SDL_Init(SDL_INIT_VIDEO);
    _renderWindow = SDL_CreateWindow("QuickerDSDA",  SDL_WINDOWPOS_UNDEFINED,  SDL_WINDOWPOS_UNDEFINED, _videoWidth, _videoHeight, 0);
    _renderer = SDL_CreateRenderer(_renderWindow, -1, SDL_RENDERER_ACCELERATED);
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, _videoWidth, _videoHeight);
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
    void *pixels = nullptr;
    int pitch = _videoPitch;

    SDL_Rect srcRect  = { 0, 0, _videoWidth, _videoHeight };
    SDL_Rect destRect = { 0, 0, _videoWidth, _videoHeight };

    if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) < 0) return;
    memcpy(pixels, _videoBuffer, 4 * _videoHeight * _videoWidth);
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
  }

  void deserializeState(jaffarCommon::deserializer::Base& d) 
  {
  }

  size_t getVideoBufferSize() const { return _videoBufferSize; }
  uint8_t* getVideoBufferPtr() const { return (uint8_t*)_videoBuffer; }

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


  unsigned int _skill; 
  unsigned int _episode;
  unsigned int _map;
  unsigned int _compatibilityLevel;
  bool _fastMonsters;
  bool _monstersRespawn;
  bool _noMonsters;

  std::unique_ptr<jaffar::InputParser> _inputParser;
  static uint32_t InputGetter(void* inputValue) { return *(uint32_t*)inputValue; }

  // Rendering stuff
  int _videoWidth;
  int _videoHeight;
  int _videoPitch;
  SDL_Window* _renderWindow;
  SDL_Renderer* _renderer;
  SDL_Texture* _texture;
  SDL_Surface* _baseSurface;
  void* _videoBuffer;
  size_t _videoBufferSize;
  bool _renderingEnabled = false;
};

} // namespace jaffar