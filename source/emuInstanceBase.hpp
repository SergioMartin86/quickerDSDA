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

#define VIDEO_HORIZONTAL_PIXELS 160
#define	VIDEO_VERTICAL_PIXELS 144

extern "C"
{
  int old_main(int argc, char **argv);
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

    // Allocating video buffer
    _videoBuffer = (uint32_t*)malloc(sizeof(uint32_t) * VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS);
  }

  virtual ~EmuInstanceBase() 
  {
    free(_videoBuffer);
  }

  virtual void advanceState(const jaffar::input_t &input)
  {

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
  }

  void initializeVideoOutput()
  {
    char* argv[] = { "dsda", "-iwad", "wads/freedoom1.wad" };
    old_main(3, argv);
    // SDL_Init(SDL_INIT_VIDEO);
    // _renderWindow = SDL_CreateWindow("QuickerDSDA",  SDL_WINDOWPOS_UNDEFINED,  SDL_WINDOWPOS_UNDEFINED, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, 0);
    // _renderer = SDL_CreateRenderer(_renderWindow, -1, SDL_RENDERER_ACCELERATED);
    // _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
  }

  void finalizeVideoOutput()
  {
    // SDL_DestroyTexture(_texture);
    // SDL_DestroyRenderer(_renderer);
    // SDL_DestroyWindow(_renderWindow);
    // SDL_Quit();
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
    // void *pixels = nullptr;
    // int pitch = 0;

    // SDL_Rect srcRect  = { 0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS };
    // SDL_Rect destRect = { 0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS };

    // if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) < 0) return;
    // memcpy(pixels, _videoBuffer, sizeof(uint32_t) * VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS);
    // // memset(pixels, (32 << 24) + (32 << 16) + (32 << 8) + 32, sizeof(uint32_t) * VIDEO_VERTICAL_PIXELS * VIDEO_HORIZONTAL_PIXELS);
    // SDL_UnlockTexture(_texture);
    // SDL_RenderClear(_renderer);
    // SDL_RenderCopy(_renderer, _texture, &srcRect, &destRect);
    // SDL_RenderPresent(_renderer);
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
  SDL_Window* _renderWindow;
  SDL_Renderer* _renderer;
  SDL_Texture* _texture;
  uint32_t* _videoBuffer;
  size_t _videoBufferSize;
  bool _renderingEnabled = false;
};

} // namespace jaffar