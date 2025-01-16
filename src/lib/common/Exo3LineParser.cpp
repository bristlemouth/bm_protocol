//
// Created by Evan Shapiro on 10/25/24.
//

#include "Exo3LineParser.h"
#include "string.h"

Exo3DataLineParser::Exo3DataLineParser(size_t numValues, const char* header)
    : OrderedSeparatorLineParser("+-", 256, nullptr, numValues + 1, header) {
  // Allocate and set up `valueTypes` based on `num_values`, add one for response identifier header
  auto value_types = new ValueType[numValues + 1];
  value_types[0] = TYPE_INVALID;  // First value as placeholder if needed
  for (size_t i = 0; i < numValues; ++i) {
    value_types[i+1] = TYPE_DOUBLE;  // Set all subsequent values as TYPE_DOUBLE
  }
  _valueTypes = value_types;
}


Exo3DataLineParser::~Exo3DataLineParser() {
  delete[] _valueTypes;  // Free dynamically allocated `value_types` array
}

bool Exo3DataLineParser::parseValueFromToken(const char* token, size_t index, char foundSeparator) {
  // If there's no separator, call the parent method directly with the token
  if (foundSeparator == '\0') {
    return OrderedSeparatorLineParser::parseValueFromToken(token, index, foundSeparator);
  }
  // Buffer to hold separator + token (only allocate if separator is present)
  const size_t token_len = strlen(token);
  if (token_len >= 255) {  // Limit size to avoid overflows
    printf("ERR - token too large!\n");
    return false;
  }
  char token_copy[2 + token_len];  // 1 for separator + token_len + 1 for null terminator
  // Insert the '+' or '-' separator at start. `strto` functions in parent
  //  OrderedSeparatorLineParser::parseValueFromToken know how to handle those.
  token_copy[0] = foundSeparator;
  // Copy token and null-terminator
  memcpy(&token_copy[1], token, token_len + 1);
  // Pass the combined string to the parent parser.
  return OrderedSeparatorLineParser::parseValueFromToken(token_copy, index, foundSeparator);
}
