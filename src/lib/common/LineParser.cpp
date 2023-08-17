//
// Created by Evan Shapiro on 7/9/23.
//

#include "LineParser.h"
#include "string.h"
#include "util.h"
#include <cstdlib>

LineParser::LineParser(const char* separator, size_t maxLineLen, ValueType* valueTypes, size_t numValues) :
  _values(nullptr),
  _valueTypes(valueTypes),
  _separator(separator),
  _maxLineLen(maxLineLen),
  _numValues(numValues) {}

bool LineParser::init() {
  _values = static_cast<Value *>(pvPortMalloc(sizeof(Value) * _numValues));
  if (_values != NULL) {
    for (size_t i = 0; i < _numValues; ++i) {
      _values[i].type = _valueTypes[i];
    }
    return true;
  }
  return false;
}

bool LineParser::parseLine(const char* line, uint16_t len) {
  // TODO verify line is 0-terminated.
  if (_values == nullptr) {
    printf("ERR Parser values uninitialized!\n");
    return false;
  }
  // Create a mutable copy for strtok_r to mangle
  // add an extra byte to ensure 0 termination
  char* work_str = static_cast<char *>(pvPortMalloc(len + 1));
  configASSERT(work_str);
  // ensure last byte of strtok working buffer is NULL so it doesn't march off into the sunset.
  memset(work_str, 0, len + 1);
  memcpy(work_str, line, len);
  // Create a pointer to free when we're done, need to clobber work_str for strtok_r.
  char* free_work_str = work_str;

  // Parse the values
  for (size_t i = 0; i < _numValues; ++i) {
    char* saveptr; // Additional pointer for strtok_r
    char* token = strtok_r(work_str, _separator, &saveptr);
    if (token == NULL) {
      vPortFree(free_work_str);
      return false; // No separator found
    }

    // Parse the value based on its type
    switch (_values[i].type) {
      case TYPE_UINT64:
        _values[i].data.uint64_val = strtoull(token, NULL, 10);
        break;
      case TYPE_INT64:
        _values[i].data.int64_val = strtoll(token, NULL, 10);
        break;
      case TYPE_DOUBLE:
        _values[i].data.double_val = strtod(token, NULL);
        break;
      default:
        vPortFree(free_work_str);
        return false; // Unknown type
    }
    // Prepare for the next strtok_r call
    work_str = NULL;
  }
  vPortFree(free_work_str);
  return true;
}

Value LineParser::getValue(u_int16_t index) {
  Value retVal = {TYPE_INVALID, {0}};

  if (_values == nullptr) {
    printf("ERR Parser values uninitialized!\n");
  }
  else if (index >= _numValues) {
    printf("ERR Parsed value at %u out of range %zu!\n", index, _numValues);
  }
  else {
    retVal = _values[index];
  }
  return retVal;
}
