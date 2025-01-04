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

struct input_t
{
};

class InputParser
{
public:

  enum controller_t { lmp, jaffar };

  InputParser(const nlohmann::json &config)
  {
    // Parsing controller type
    {
      bool isTypeRecognized = false;

      const auto controllerType = jaffarCommon::json::getString(config, "Controller Type");
      if (controllerType == "Demo LMP") { _controllerType = controller_t::lmp; isTypeRecognized = true; }
      if (controllerType == "Jaffar")   { _controllerType = controller_t::jaffar;  isTypeRecognized = true; }
      
      if (isTypeRecognized == false) JAFFAR_THROW_LOGIC("Controller type not recognized: '%s'\n", controllerType.c_str()); 
   }
  }

  inline input_t parseInputString(const std::string &inputString) const
  {
    // Storage for the input
    input_t input;

    // Returning input
    return input;
  };

  private:


  static inline void reportBadInputString(const std::string &inputString, const char c)
  {
    JAFFAR_THROW_LOGIC("Could not decode input string: '%s' - Read: '%c'\n", inputString.c_str(), c);
  }

  input_t _input;
  controller_t _controllerType;
}; // class InputParser

} // namespace jaffar