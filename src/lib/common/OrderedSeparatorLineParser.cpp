//
// Created by Evan Shapiro on 8/17/23.
//

#include "OrderedSeparatorLineParser.h"
#include "string.h"

OrderedSeparatorLineParser::OrderedSeparatorLineParser(const char* separator, size_t maxLineLen,
                                                       const ValueType* valueTypes, size_t numValues, const char* header /*= nullptr*/)
    : LineParser(separator, maxLineLen, valueTypes, numValues, header) { }

bool OrderedSeparatorLineParser::parseValues(char* workStr) {
  char* tokenStart = workStr;
  char valueSeparator = '\0';

  for (size_t i = 0; i < _numValues; i++) {
    if (tokenStart == nullptr) {
      printf("ERR - no token to parse from!\n");
      return false;
    }
    // Find the first occurrence of any separator character. strpbrk looks for the first
    //  occurence of any of the target characters in the set of characters in the _separator
    //  string. It does not match multi-character patterns.
    char* sepPos = strpbrk(tokenStart, _separator);
    // Some parsers need to interpret the separator, so we'll save the detected parser for them.
    char nextSeparator = '\0';
    if (sepPos) {
      nextSeparator = *sepPos;
      *sepPos = '\0'; // Split the string at the separator for parsing.
    } else if (i < _numValues-1) {
      // Error if we don't find the next separator and we're not parsing the last value.
      printf("ERR - expected next separator: \"%s\" not found in line!\n", _separator);
      return false;
    }
    // Parse the value at the current position and pass along the previously saved separator
    if (!parseValueFromToken(tokenStart, i, valueSeparator)) {
      return false;
    }
    // Save the next separator in outer scope so can use when parsing next value
    if (sepPos != nullptr) {
      valueSeparator = nextSeparator;
      // Move to the next part of the string after the separator
      tokenStart = sepPos + 1;
    } else {
      valueSeparator = '\0';
      tokenStart = nullptr;
    }
  }
  return true;
}
