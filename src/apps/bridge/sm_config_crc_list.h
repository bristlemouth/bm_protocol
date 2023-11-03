#include "abstract_configuration.h"

class SMConfigCRCList {
public:
  SMConfigCRCList(cfg::AbstractConfiguration *cfg);

  static constexpr char KEY[] = "smConfigurationCrc";
  static constexpr size_t KEY_LEN = sizeof(KEY) - 1;

  // MAX_CONFIG_BUFFER_SIZE_BYTES in configuration.h
  static constexpr size_t MAX_BUFFER_SIZE = 50;

  bool contains(uint32_t crc);

private:
  cfg::AbstractConfiguration *_cfg;
};
