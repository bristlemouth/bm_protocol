#include "abstract_configuration.h"

class SMConfigCRCList {
public:
  SMConfigCRCList(cfg::AbstractConfiguration *cfg);

  static constexpr char KEY[] = "smConfigurationCrc";
  static constexpr size_t KEY_LEN = sizeof(KEY) - 1;

  // MAX_CONFIG_BUFFER_SIZE_BYTES in configuration.h
  static constexpr size_t MAX_BUFFER_SIZE = 50;

  // The maximum number of CRCs that can be stored within MAX_BUFFER_SIZE
  static constexpr size_t MAX_LIST_SIZE = 9;

  bool contains(uint32_t crc);
  void add(uint32_t crc);
  void clear();
  uint32_t *get(uint32_t &num_crcs);

private:
  cfg::AbstractConfiguration *_cfg;
  uint32_t _crc_list[MAX_LIST_SIZE];
  size_t _num_crcs;

  void decode();
  void encode();
};
