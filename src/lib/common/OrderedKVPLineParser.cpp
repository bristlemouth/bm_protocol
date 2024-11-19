//
// Created by Evan Shapiro on 8/17/23.
//

#include "OrderedKVPLineParser.h"
#include "string.h"
#include <cstdlib>

OrderedKVPLineParser::OrderedKVPLineParser(const char *separator, size_t maxLineLen,
                                           const ValueType *valueTypes, size_t numValues,
                                           const char **keys, const char *header /*= nullptr*/)
    : LineParser(separator, maxLineLen, valueTypes, numValues, header), _keys(keys) {}

// Parse the value based on its type
bool OrderedKVPLineParser::parseValues(char *workStr) {
  // Parse the values
  for (size_t i = 0; i < _numValues; i++) {
    //    printf("DEBUG - searching for key: %s\n", _keys[i]);

    char *pos = strstr(workStr, _keys[i]);
    if (pos == nullptr) {
      printf("ERR - Next key %s not found in line!: %s\n", _keys[i], workStr);
      return false;
    } else {
      // We found the expected key. Move the working pointer to character after it.
      workStr = pos + strlen(_keys[i]);
    }

    char *saveptr; // Additional pointer for strtok_r
    // find separator between target value and next key
    char *token = strtok_r(workStr, _separator, &saveptr);
    if (token == NULL) {
      printf("ERR - expected next separator: \"%c\" not found in line!\n", _separator[0]);
      return false; // No separator found
    }
    //    printf("DEBUG - found token: %s\n", token);
    // Parse the value at specified index
    if (!parseValueFromToken(token, i, '\0')) {
      return false;
    }
    // Prepare for the next strstr call
    workStr = saveptr;
  }

  return true;
}
