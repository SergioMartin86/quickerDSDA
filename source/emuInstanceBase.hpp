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
#include <d_player.h>
#include <w_wad.h>

#ifdef _ENABLE_RENDERING
#include <SDL.h>
#endif

extern "C"
{
  // Pulled in for the config-id enum and the (C-linked) config setter, used to
  // disable DSDA's auto-keyframe (rewind autosave) feature in headless mode.
  #include <dsda/configuration.h>

  // Pulled in for the deep full-state verification hash (getDeepStateHash):
  // the thinker list and the sector/line tables.
  #include <p_tick.h>
  #include <r_state.h>

  int headlessMain(int argc, char **argv);

  #ifdef _DEGLOB_TLS_INIT
  void __deglob_init_tls(void);   // generated per-thread TLS-pointer initializer
  #endif
  void headlessRunSingleTick();
  void headlessUpdateSounds(void);
  void headlessClearTickCommand();
  void headlessSetTickCommand(int playerId, int forwardSpeed, int strafingSpeed, int turningSpeed, int fire, int action, int weapon, int altWeapon);

  // Rendering-related functions
  #ifdef _ENABLE_RENDERING
  void headlessUpdateVideo(void);
  void* headlessGetVideoBuffer();
  int headlessGetVideoPitch();
  int headlessGetVideoWidth();
  int headlessGetVideoHeight();
  void headlessEnableVideoRendering();
  void headlessDisableVideoRendering();
  void headlessEnableAudioRendering();
  void headlessDisableAudioRendering();
  uint32_t* headlessGetPallette();
  unsigned char * I_CaptureAudio (int* nsamples);
  void I_InitSound(void);
  void I_SetSoundCap (void);
  #endif

  void headlessSetSaveStatePointer(void* savePtr, int saveStateSize);
  size_t headlessGetEffectiveSaveSize();
  void dsda_ArchiveAll(void);
  void dsda_UnArchiveAll(void);
  void headlessGetMapName(char* outString);

  void D_AddFile (const char *file, wad_source_t source, void* const buffer, const size_t size);
  void AddIWAD(const char *iwad, void* const buffer, const size_t size);
}

// Players information
extern "C" __STORAGE_MODIFIER int enableOutput;
extern "C" __STORAGE_MODIFIER player_t players[MAX_MAXPLAYERS];
extern "C" __STORAGE_MODIFIER int preventLevelExit;
extern "C" __STORAGE_MODIFIER int preventGameEnd;
extern "C" __STORAGE_MODIFIER int reachedLevelExit;
extern "C" __STORAGE_MODIFIER int reachedGameEnd;
extern "C" __STORAGE_MODIFIER int gamemap;
extern "C" __STORAGE_MODIFIER int gametic;
extern "C" __STORAGE_MODIFIER dboolean playeringame[MAX_MAXPLAYERS];
extern "C" __STORAGE_MODIFIER int consoleplayer;
extern "C" __STORAGE_MODIFIER int displayplayer;
extern "C" __STORAGE_MODIFIER ticcmd_t local_cmds[MAX_MAXPLAYERS];

namespace jaffar
{

// #define SAVEGAMESIZE 0x100000

class EmuInstanceBase
{
  public:

  EmuInstanceBase(const nlohmann::json &config)
  {
    // Getting IWAD File Path
    _IWADFilePath = jaffarCommon::json::getString(config, "IWAD File Path");

    // Getting expected IWAD SHA1 hash
    _expectedIWADSHA1 = jaffarCommon::json::getString(config, "Expected IWAD SHA1");

    // Getting maximum state size
    _stateSize = jaffarCommon::json::getNumber<size_t>(config, "State Size");
 
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

    _playerPointOfView = jaffarCommon::json::getNumber<uint8_t>(config, "Player Point of View");

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
    if (jaffarCommon::file::loadStringFromFile(_IWADFileDataBuffer, _IWADFilePath) == false) JAFFAR_THROW_LOGIC("Could not IWAD file: %s\n", _IWADFilePath.c_str());

    // Calculating IWAD SHA1
    auto IWADSHA1 = jaffarCommon::hash::getSHA1String(_IWADFileDataBuffer);

    // Checking with the expected SHA1 hash
    if (IWADSHA1 != _expectedIWADSHA1) JAFFAR_THROW_LOGIC("Wrong IWAD SHA1. Found: '%s', Expected: '%s'\n", IWADSHA1.c_str(), _expectedIWADSHA1.c_str());

    // Loading IWAD into DSDA
    AddIWAD(_IWADFilePath.c_str(), _IWADFileDataBuffer.data(), _IWADFileDataBuffer.size());

    // Loading PWAD Files
    _PWADFileDataBuffers.resize(_PWADFilePaths.size());
    for (size_t i = 0; i < _PWADFilePaths.size(); i++)
    {
      // Loading PWAD File
      if (jaffarCommon::file::loadStringFromFile(_PWADFileDataBuffers[i], _PWADFilePaths[i]) == false) JAFFAR_THROW_LOGIC("Could not PWAD file: %s\n", _PWADFilePaths[i].c_str());

      // Calculating IWAD SHA1
      auto PWADSHA1 = jaffarCommon::hash::getSHA1String(_PWADFileDataBuffers[i]);

      // Checking with the expected SHA1 hash
      if (PWADSHA1 != _PWADExpectedSHA1s[i]) JAFFAR_THROW_LOGIC("Wrong PWAD SHA1. Found: '%s', Expected: '%s'\n", PWADSHA1.c_str(), _PWADExpectedSHA1s[i].c_str());

      // Loading IWAD into DSDA
      D_AddFile(_PWADFilePaths[i].c_str(), source_pwad, _PWADFileDataBuffers[i].data(), _PWADFileDataBuffers[i].size());
    }

    // Creating arguments
    int argc = 0;
    char** argv = (char**) malloc (sizeof(char*) * 512);
    
    // Specifying executable name
    char arg0[] = "dsda";
    argv[argc++] = arg0;

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

    // Setting players in game
    playeringame[0] = _player1Present;
    playeringame[1] = _player2Present;
    playeringame[2] = _player3Present;
    playeringame[3] = _player4Present;

    // Getting player count
    auto playerCount = _player1Present + _player2Present + _player3Present + _player4Present;
    char arg9[] = "-solo-net";
    if (playerCount > 1) argv[argc++] = arg9;

    // Initializing DSDA core
    headlessMain(argc, argv);

    #ifdef _DEGLOB_TLS_INIT
    // Per-thread initialization of thread-local pointers/tables whose original
    // static initializers took the address of a now-thread-local global (e.g.
    // the intercepts_overrun emulation table -> &bmapwidth/&playerstarts/...).
    // Generated by the deglobalizer; compiled in only for the new core.
    __deglob_init_tls();
    #endif

    // Disabling DSDA's auto-keyframe (rewind autosave) feature. In headless TAS
    // mode JaffarPlus drives its own save/load, so the per-tic keyframe ring is
    // pure overhead -- and its P_InitSaveBuffer/Z_Malloc path corrupts the zone
    // heap here. Setting the depth config to 0 triggers dsda_InitKeyFrame, which
    // tears down the ring (auto_kf_size = 0) so dsda_UpdateAutoKeyFrames no-ops.
    dsda_UpdateIntConfig(dsda_config_auto_key_frame_depth, 0, false);

    #ifdef _ENABLE_RENDERING

    // Getting video information
    _videoSource = (uint8_t*)headlessGetVideoBuffer();
    _videoWidth  = headlessGetVideoWidth();
    _videoHeight = headlessGetVideoHeight();
    _videoPitch  = headlessGetVideoPitch();

    // Calculating video buffer size
    int pixelBytes = 4; // RGB32
    _videoBufferSize = _videoWidth * _videoHeight * pixelBytes;

    // Allocating buffer
    _videoBuffer = (uint32_t*) calloc (_videoBufferSize, 1);

    #endif

    // Setting save state size
    _saveData = (uint8_t*)malloc(_stateSize);

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
    // headlessClearTickCommand();
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

    #ifdef _ENABLE_RENDERING

    // If rendering is enabled, update vid now
    if(_renderingEnabled == true) if (players[0].mo != nullptr)
    {
      displayplayer = consoleplayer = _playerPointOfView;
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

     	int nSamples = 0;
      void* audioBuffer = nullptr;
      audioBuffer = I_CaptureAudio(&nSamples);
    }

    #endif

    if (reachedLevelExit == 1) jaffarCommon::logger::log("[] Level Exit detected on tic:   %d\n", gametic);
    if (reachedGameEnd   == 1) jaffarCommon::logger::log("[] Game End detected on tic:   %d\n", gametic);
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

  // Deep full-state verification hash: every mobj's physics fields + every
  // sector's mutable fields + the RNG. Far stronger than getStateHash (which is
  // only player[0]); used to prove that a savestate trim or serialization change
  // does not perturb ANY gameplay state, not just the player. Not on the hot path.
  inline jaffarCommon::hash::hash_t getDeepStateHash() const
  {
    MetroHash128 hash;
    hash.Update(gamemap);
    hash.Update(gametic);

    // The mobj thinker function, obtained at runtime from the player's own mobj
    // (which is one) so we need no C-linkage symbol reference to P_MobjThinker.
    const think_t mobjFn = (players[0].mo != nullptr) ? players[0].mo->thinker.function : nullptr;

    // All mobj thinkers, in list order, physics fields only.
    for (thinker_t *th = thinkercap.next; th != &thinkercap; th = th->next)
    {
      if (mobjFn == nullptr || th->function != mobjFn) continue;
      const mobj_t *m = (const mobj_t *)th;
      hash.Update(m->x);    hash.Update(m->y);    hash.Update(m->z);
      hash.Update(m->momx); hash.Update(m->momy); hash.Update(m->momz);
      hash.Update(m->angle); hash.Update(m->health); hash.Update(m->type);
      hash.Update(m->flags); hash.Update(m->tics);  hash.Update(m->frame);
      hash.Update(m->sprite); hash.Update(m->reactiontime); hash.Update(m->movecount);
      hash.Update(m->movedir); hash.Update(m->threshold);
      hash.Update((int)(m->state ? (m->state - states) : -1));
    }

    // All sectors, GAMEPLAY-relevant mutable fields only. floorpic/ceilingpic
    // (flats) and lightlevel are render-only for Doom2 (a sky-flat ceiling does
    // affect projectiles, but that consequence shows up in the mobj hash above),
    // so they are excluded here -- letting us trim them from the savestate while
    // still catching any real gameplay divergence via mobjs + heights + special.
    for (int i = 0; i < numsectors; i++)
    {
      const sector_t *s = &sectors[i];
      hash.Update(s->floorheight);   hash.Update(s->ceilingheight);
      hash.Update(s->floorpic);      hash.Update(s->ceilingpic);
      hash.Update(s->lightlevel);    hash.Update(s->special);
    }

    jaffarCommon::hash::hash_t result;
    hash.Finalize(reinterpret_cast<uint8_t *>(&result));
    return result;
  }

  int getMapNumber () const { return gamemap; }
  bool isLevelExit () const { return reachedLevelExit == 1; }
  bool isGameEnd () const { return reachedGameEnd == 1; }

  static float getFloatFrom1616Fixed(const fixed_t value)
  {
    double integerPart = (double)(value >> FRACBITS);
    double floatPart = (double)((value << FRACBITS) >> FRACBITS) / 65535.0;
    return (float) (integerPart + floatPart);
  }

  void printInformation()
  {
    char mapName[512];
    headlessGetMapName(mapName);
    jaffarCommon::logger::log("[] Map:        %s\n", mapName);
    jaffarCommon::logger::log("[] Game Tic:   %d\n", gametic);
    jaffarCommon::logger::log("[] Level Exit: %s\n", reachedLevelExit == 1 ? "Yes" : "No");
    jaffarCommon::logger::log("[] Game End:   %s\n", reachedGameEnd   == 1 ? "Yes" : "No");
    jaffarCommon::logger::log("[] Players:    %1d%1d%1d%1d\n", playeringame[0], playeringame[1], playeringame[2], playeringame[3]);

    if (players[0].mo != nullptr)
    {
      jaffarCommon::logger::log("[] Player 1 Coordinates:    (%f, %f, %f)\n", getFloatFrom1616Fixed(players[0].mo->x), getFloatFrom1616Fixed(players[0].mo->y), getFloatFrom1616Fixed(players[0].mo->z));
      jaffarCommon::logger::log("[] Player 1 Angle:           %lu\n", players[0].mo->angle);
      jaffarCommon::logger::log("[] Player 1 Momenta:        (%f, %f, %f)\n", getFloatFrom1616Fixed(players[0].mo->momx), getFloatFrom1616Fixed(players[0].mo->momy), getFloatFrom1616Fixed(players[0].mo->momz));
      jaffarCommon::logger::log("[] Player 1 Health:          %d\n", players[0].mo->health);
    }
  }

  void initializeVideoOutput()
  {
    #ifdef _ENABLE_RENDERING
    SDL_Init(SDL_INIT_VIDEO);
    _renderWindow = SDL_CreateWindow("QuickerDSDA",  SDL_WINDOWPOS_UNDEFINED,  SDL_WINDOWPOS_UNDEFINED, _videoWidth, _videoHeight, 0);
    _renderer = SDL_CreateRenderer(_renderWindow, -1, SDL_RENDERER_ACCELERATED);
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, _videoWidth, _videoHeight);
    #endif
  }

  void finalizeVideoOutput()
  {
    #ifdef _ENABLE_RENDERING
    SDL_DestroyTexture(_texture);
    SDL_DestroyRenderer(_renderer);
    SDL_DestroyWindow(_renderWindow);
    SDL_Quit();
    #endif
  }

  void enableRendering()
  {
    #ifdef _ENABLE_RENDERING
    headlessEnableVideoRendering();
    headlessEnableAudioRendering();
    I_SetSoundCap();
    I_InitSound();
    _renderingEnabled = true;
    #endif
  }

  void disableRendering()
  {
    #ifdef _ENABLE_RENDERING
    headlessDisableVideoRendering();
    headlessDisableAudioRendering();
    _renderingEnabled = false;
    #endif
  }

  void updateRenderer()
  {
    #ifdef _ENABLE_RENDERING
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
    #endif
  }

  inline size_t getStateSize() const
  {
    // serializeState() pushes the full _stateSize archive buffer plus the
    // local tick commands, so the reported state size must include both or
    // callers under-allocate and the (de)serializer overruns its buffer.
    return _stateSize + sizeof(local_cmds);
  }

  inline jaffar::InputParser *getInputParser() const { return _inputParser.get(); }
  
  void serializeState(jaffarCommon::serializer::Base& s) const
  {
    headlessSetSaveStatePointer(_saveData, _stateSize);
    dsda_ArchiveAll();
    s.push(_saveData, _stateSize);
    s.push(local_cmds, sizeof(local_cmds));
  }

  void deserializeState(jaffarCommon::deserializer::Base& d) 
  {
    d.pop(_saveData, _stateSize);
    headlessSetSaveStatePointer(_saveData, _stateSize);
    dsda_UnArchiveAll();
    d.pop(local_cmds, sizeof(local_cmds));
  }

  size_t getVideoBufferSize() const
  {
    #ifdef _ENABLE_RENDERING
     return _videoBufferSize;
    #endif
     return 0;
  }

  uint8_t* getVideoBufferPtr() const
  {
    #ifdef _ENABLE_RENDERING
     return (uint8_t*)_videoBuffer;
    #endif
     return nullptr;
  }
  
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
  uint8_t* _saveData;

  private:

  std::string _IWADFilePath;
  std::string _IWADFileDataBuffer;
  std::string _expectedIWADSHA1;

  std::vector<std::string> _PWADFilePaths;
  std::vector<std::string> _PWADFileDataBuffers;
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
  uint8_t _playerPointOfView;
  uint8_t _playerCount;

  std::unique_ptr<jaffar::InputParser> _inputParser;
  static uint32_t InputGetter(void* inputValue) { return *(uint32_t*)inputValue; }

  // Rendering stuff
  #ifdef _ENABLE_RENDERING
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
  #endif
};

} // namespace jaffar