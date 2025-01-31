#pragma once

// Base controller class
// by eien86

#include <cstdint>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/json.hpp>
#include <string>
#include <sstream>

namespace jaffar
{

#define _MAX_PLAYERS 4

struct playerInput_t
{
  int8_t forwardSpeed = 0;
  int8_t strafingSpeed = 0;
  int8_t turningSpeed = 0;
  bool fire = false;
  bool action = false;
  uint8_t weapon = 0;
  bool altWeapon = false;
};

typedef std::array<playerInput_t, _MAX_PLAYERS> input_t;

class InputParser
{
public:

  enum controller_t { jaffar };

  InputParser(const nlohmann::json &config)
  {
    // Parsing controller type
    {
      bool isTypeRecognized = false;

      const auto controllerType = jaffarCommon::json::getString(config, "Controller Type");
      if (controllerType == "Jaffar")   { _controllerType = controller_t::jaffar;  isTypeRecognized = true; }
      
      if (isTypeRecognized == false) JAFFAR_THROW_LOGIC("Controller type not recognized: '%s'\n", controllerType.c_str()); 

      _playerCount = jaffarCommon::json::getNumber<uint8_t>(config, "Player Count");
   }
  }

  inline input_t parseInputString(const std::string &inputString) const
  {
    // Storage for the input
    input_t input;

    // Converting input into a stream for parsing
    std::istringstream ss(inputString);

    // Start separator
    char c = ss.get();
    if (c != '|') reportBadInputString(inputString, c);

    // Parsing controller 1 inputs
    for (uint8_t i = 0; i < _playerCount; i++) parsePlayerInputs(input[i], ss, inputString);

    // End separator
    c = ss.get();
    if (c != '|') reportBadInputString(inputString, c);

    // If its not the end of the stream, then extra values remain and its invalid
    c = ss.get();
    if (ss.eof() == false) reportBadInputString(inputString, c);

    // Returning input
    return input;
  };

  private:

  static inline void parsePlayerInputs(playerInput_t& input, std::istringstream& ss, const std::string& inputString)
  {
    // Controller separator
    char c = ss.get();
    if (c != '|') reportBadInputString(inputString, c);

    // Parsing forward speed
    char forwardSpeedString[5];
    forwardSpeedString[0] = ss.get();
    forwardSpeedString[1] = ss.get();
    forwardSpeedString[2] = ss.get();
    forwardSpeedString[3] = ss.get();
    forwardSpeedString[4] = '\0';
    input.forwardSpeed = atoi(forwardSpeedString);

    // Parsing comma
    c = ss.get();
    if (c != ',') reportBadInputString(inputString, c);

    // Parsing strafing speed
    char strafingSpeedString[5];
    strafingSpeedString[0] = ss.get();
    strafingSpeedString[1] = ss.get();
    strafingSpeedString[2] = ss.get();
    strafingSpeedString[3] = ss.get();
    strafingSpeedString[4] = '\0';
    input.strafingSpeed = atoi(strafingSpeedString);
    
    // Parsing comma
    c = ss.get();
    if (c != ',') reportBadInputString(inputString, c);

    // Parsing turning speed
    char turningSpeedString[5];
    turningSpeedString[0] = ss.get();
    turningSpeedString[1] = ss.get();
    turningSpeedString[2] = ss.get();
    turningSpeedString[3] = ss.get();
    turningSpeedString[4] = '\0';
    input.turningSpeed = atoi(turningSpeedString);

    // Parsing comma
    c = ss.get();
    if (c != ',') reportBadInputString(inputString, c);

    // Parsing weapon select speed
    char weaponSelectString[5];
    weaponSelectString[0] = ss.get();
    weaponSelectString[1] = ss.get();
    weaponSelectString[2] = ss.get();
    weaponSelectString[3] = ss.get();
    weaponSelectString[4] = '\0';
    input.weapon = atoi(weaponSelectString);

    // Parsing comma
    c = ss.get();
    if (c != ',') reportBadInputString(inputString, c);
    
    // Fire
    c = ss.get();
    if (c != '.' && c != 'F') reportBadInputString(inputString, c);
    if (c == 'F') input.fire = true;

    // Action
    c = ss.get();
    if (c != '.' && c != 'A') reportBadInputString(inputString, c);
    if (c == 'A') input.action = true;

    // Alt Weapon indicator
    c = ss.get();
    if (c != '.' && c != 'X') reportBadInputString(inputString, c);
    if (c == 'X') input.altWeapon = true;
  }

  static inline void reportBadInputString(const std::string &inputString, const char c)
  {
    JAFFAR_THROW_LOGIC("Could not decode input string: '%s' - Read: '%c' (%u)\n", inputString.c_str(), c, (uint8_t)c);
  }

  uint8_t _playerCount;
  controller_t _controllerType;
}; // class InputParser

} // namespace jaffar