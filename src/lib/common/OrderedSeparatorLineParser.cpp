//
// Created by Evan Shapiro on 8/17/23.
//

#include "OrderedSeparatorLineParser.h"
#include "string.h"
#include <cstdlib>

OrderedSeparatorLineParser::OrderedSeparatorLineParser(const char* separator, size_t maxLineLen,
                                                       const ValueType* valueTypes, size_t numValues, const char* header /*= nullptr*/)
    : LineParser(separator, maxLineLen, valueTypes, numValues, header) { }

// Parse the value based on its type
// workStr is a copy of the line - note strtok_r will modify *workStr in-place
bool OrderedSeparatorLineParser::parseValues(char* workStr) {
  char* saveptr; // Additional pointer for strtok_r so it stays thread-safe
  // Parse number of expected values from the string
  for (size_t i = 0; i < _numValues; ++i) {
    // First call to strok_r, workStr will be non-NULL (start of string).
    //    strtok_r will set first occurence of _separator in workStr to NULL
    //    set saveptr to the next byte
    //    then return workStr
    // Subsequent calls workStr is NULL, telling strtok_r to start search from saveptr

    char* token = strtok_r(workStr, _separator, &saveptr);
    if (token == nullptr) {
      printf("ERR - expected next separator: \"%c\" not found in line!\n", _separator[0]);
      return false; // No separator found
    }
    // Parse the value at specified index
    if (!parseValueFromToken(token, i)) {
      return false;
    }
    // Prepare for the next strtok_r call
    workStr = nullptr;
  }

  return true;
}
