
#include <stdint.h>

#include "enumToStr.h"

/*

Example LUT:

static const enumStrLUT_t exampleLUT[] = {
  {kEnumValue1, "Enum Value 1!"},
  {kEnumValue2, "Something else"},
  {kEnumValue3, ""}, // Note that we don't use a NULL, but instead an empty string
  {kEnumValue4, "Wahooo!"},
   ...
  {0, NULL}, // MUST be NULL terminated list otherwise things WILL break
};

Example use:
    const char *str = enumToStr(exampleLUT, kEnumValueX);
*/

/*!
  Get string corresponding to enum value from lookup table

  NOTE: LUT MUST have a {0, NULL} item at the end, otherwise this function
  will not behave properly!

  \param[in] *lut - Pointer to lookup table
  \param[in] val - enum value, cast as uint32
  \return enum string if found, emptry string "" otherwise
*/
const char *enumToStr(const enumStrLUT_t *lut, uint32_t val) {
  const char* rval = "";

  while(lut && lut->str) {
    if(lut->val == val) {
      rval = lut->str;
      break;
    }
    lut++;
  }

  return rval;
}
