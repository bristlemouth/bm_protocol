#include "fff.h"
#include "sm_config_crc_list.h"
#include "gtest/gtest.h"

DEFINE_FFF_GLOBALS;

TEST(SMConfigCRCListTests, Constructor) {
  SMConfigCRCList sm_config_crc_list(cfg::Configuration * _cfg);
}
