//
// Created by Evan Shapiro on 8/17/23.
//

#include "OrderedSeparatorLineParser.h"
#include "string.h"
#include <cstdlib>

OrderedSeparatorLineParser::OrderedSeparatorLineParser(const char* separator, size_t maxLineLen,
                                                       ValueType* valueTypes, size_t numValues, const char* header /*= nullptr*/)
    : LineParser(separator, maxLineLen, valueTypes, numValues, header) { }

// Parse the value based on its type
bool OrderedSeparatorLineParser::parseValues(char* workStr) {
  // Parse the values
  for (size_t i = 0; i < _numValues; ++i) {
    char* saveptr; // Additional pointer for strtok_r
    char* token = strtok_r(workStr, _separator, &saveptr);
    if (token == NULL) {
      printf("ERR - expected next separator: \"%c\" not found in line!\n", _separator[0]);
      return false; // No separator found
    }
    // Parse the value at specified index
    if (!parseValueFromToken(token, i)) {
      return false;
    }
    // Prepare for the next strtok_r call
    workStr = NULL;
  }

  return true;
}
