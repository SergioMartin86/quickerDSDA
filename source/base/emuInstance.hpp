#pragma once

#include "../emuInstanceBase.hpp"
#include <string>
#include <vector>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>

namespace jaffar
{

class EmuInstance : public EmuInstanceBase
{
 public:

  EmuInstance(const nlohmann::json &config) : EmuInstanceBase(config)
  {
  }

  ~EmuInstance()
  {
  }

  void setWorkRamSerializationSizeImpl(const size_t size) override
  {
  }

  void enableStateBlockImpl(const std::string& block) override
  {
  }

  void disableStateBlockImpl(const std::string& block) override
  {
  }

  void doSoftReset() override
  {
  }
  
  void doHardReset() override
  {
  }

  std::string getCoreName() const override { return "Base DSDA"; }


  private:

};

} // namespace jaffar