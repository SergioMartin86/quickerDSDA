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
  
  // Video-related functions
  void headlessUpdateVideo(void);
  void* headlessGetVideoBuffer();
  int headlessGetVideoPitch();
  int headlessGetVideoWidth();
  int headlessGetVideoHeight();
  SDL_Surface* headlessGetVideoSurface();
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
  }

  virtual ~EmuInstanceBase() 
  {
  }

  virtual void advanceState(const jaffar::input_t &input)
  {
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

    // Initializing DSDA core
    char* argv[] = { "dsda", "-iwad", "wads/freedoom1.wad" };
    headlessMain(3, argv);

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
    _renderingEnabled = true;
  }

  void disableRendering()
  {
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