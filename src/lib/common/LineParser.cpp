//
// Created by Evan Shapiro on 7/9/23.
//

#include "LineParser.h"
#include "string.h"
#include "app_util.h"
#include <cstdlib>

LineParser::LineParser(const char* separator, size_t maxLineLen, const ValueType* valueTypes, size_t numValues,
                       const char* header /*= nullptr*/) :
  _values(nullptr),
  _valueTypes(valueTypes),
  _separator(separator),
  _maxLineLen(maxLineLen),
  _numValues(numValues),
  _header(header) {}

bool LineParser::init() {
  _values = static_cast<Value *>(pvPortMalloc(sizeof(Value) * _numValues));
  if (_values != NULL) {
    for (size_t i = 0; i < _numValues; i++) {
      _values[i].type = _valueTypes[i];
      if (_values[i].type == TYPE_STRING) {
        _values[i].data.string_val_ptr = nullptr;
      }
    }
    return true;
  }
  return false;
}

LineParser::~LineParser() {
  if (_values != nullptr) {
    // Free any allocated string values
    for (size_t i = 0; i < _numValues; i++) {
      if (_values[i].type == TYPE_STRING) {
        if (_values[i].data.string_val_ptr != nullptr) {
          vPortFree(_values[i].data.string_val_ptr);
          _values[i].data.string_val_ptr = nullptr;
        }
      }
    }
    vPortFree(_values);
    _values = nullptr;
  }
}

bool LineParser::parseLine(const char* line, uint16_t len) {
  // TODO verify line is less than maxLineLen - 1 (for null character)
  // TODO verify line is 0-terminated.
  if (_values == nullptr) {
    printf("ERR Parser values uninitialized!\n");
    return false;
  }
  // TODO reset all values to INVALID
  // Create a mutable copy for strtok_r to mangle
  // add an extra byte to ensure 0 termination
  char* work_str = static_cast<char *>(pvPortMalloc(len + 1));
  configASSERT(work_str);
  // ensure last byte of strtok working buffer is NULL so it doesn't march off into the sunset.
  memset(work_str, 0, len + 1);
  memcpy(work_str, line, len);
  // Create a pointer to free when we're done, need to clobber work_str for strtok_r.
  char* free_work_str = work_str;

  // If we've specified a header, verify our line contains it. TODO - verify starts with it?
  if (_header != nullptr) {
    char *pos = strstr(work_str, _header);
    if (pos != nullptr) {
      // We found the expected header. Move the working pointer past the found header.
      work_str = pos + strlen(_header);
    }
    else {
      printf("WARN - Header %s not found in line!\n", _header);
      vPortFree(free_work_str);
      return false;
    }
  }
  // Parse the values
  bool values_parsed = parseValues(work_str);
  vPortFree(free_work_str);
  return values_parsed;
}

const Value& LineParser::getValue(uint16_t index) {
  static Value invalidValue = {TYPE_INVALID, {0}};

  if (_values == nullptr) {
    printf("ERR Parser values uninitialized!\n");
    return invalidValue;
  }
  else if (index >= _numValues) {
    printf("ERR Parsed value at %u out of range %zu!\n", index, _numValues);
    return invalidValue;
  }
  else {
    return _values[index];
  }
}

bool LineParser::parseValueFromToken(const char *token, size_t index, char foundSeparator) {
  (void)(foundSeparator);
  char* endptr;
  switch (_values[index].type) {
    case TYPE_INVALID: {
      // printf("WARN - unparsable value at: %s\n", token);
      return true;
    }
    case TYPE_UINT64: {
      uint64_t parseVal = strtoull(token, &endptr, 10);
      if (endptr == token) {
        printf("ERR - failed to parse uint64 from: %s\n", token);
        return false;
      }
//      printf("DEBUG - parse val: %llu\n", parseVal);
      _values[index].data.uint64_val = parseVal;
      return true;
    }
    case TYPE_INT64: {
      int64_t parseVal = strtoll(token, &endptr, 10);
      if (endptr == token) {
        printf("ERR - failed to parse int64 from: %s\n", token);
        return false;
      }
      _values[index].data.int64_val = parseVal;
      return true;
    }
    case TYPE_DOUBLE: {
      double parseVal = strtod(token, &endptr);
      if (endptr == token) {
        printf("ERR - failed to parse double from: %s\n", token);
        return false;
      }
//      printf("DEBUG - parse val: %f\n", parseVal);
      _values[index].data.double_val = parseVal;
      return true;
    }
    case TYPE_STRING: {
      // Free any existing string to prevent memory leaks
      if (_values[index].data.string_val_ptr != nullptr) {
        vPortFree(_values[index].data.string_val_ptr);
        _values[index].data.string_val_ptr = nullptr;
      }
      // Allocate memory for the new string + \0 term char
      size_t len = strlen(token);
      _values[index].data.string_val_ptr = static_cast<char*>(pvPortMalloc(len + 1));
      configASSERT(_values[index].data.string_val_ptr);
      // Copy the token into the allocated memory
      strcpy(_values[index].data.string_val_ptr, token);
      // printf("DEBUG - parse val: %s\n", _values[index].data.string_val_ptr);
      return true;
    }
  }
  printf("ERR - Unexpected ValueType %d\n", _values[index].type);
  return false; // Unknown type
}
