//
// Created by Evan Shapiro on 7/9/23.
//

#ifndef BRISTLEMOUTH_LINEPARSER_H
#define BRISTLEMOUTH_LINEPARSER_H

#include <stdio.h>
#include <stdint.h>

// Define ValueType as an enum
typedef enum {
  TYPE_INVALID,
  TYPE_UINT64,
  TYPE_INT64,
  TYPE_DOUBLE,
  TYPE_STRING
} ValueType;

// Define ValueData as a union
union ValueData {
  uint64_t uint64_val;
  int64_t int64_val;
  double double_val;
  char* string_val_ptr;
};

// Define Value as a struct
struct Value {
  ValueType type;
  ValueData data;
};

class LineParser {
public:
  LineParser(const char* separator, size_t maxLineLen, const ValueType* valueTypes, size_t numValues,
             const char* header = nullptr);
    // NOTE - header must be NULL terminated if used!
  bool init();
  bool parseLine(const char* line, uint16_t len);
  const Value* getValues() { return (const Value*)_values; }
  const Value& getValue(uint16_t index);
protected:
  virtual ~LineParser(); // Add virtual destructor
  bool parseValueFromToken(const char* token, size_t index);
private:
  virtual bool parseValues(char* workStr) = 0;
  // parse string at token (must not be nullptr! must be NULL terminated!) and if sucessful add to _values array at i.

protected:
  Value* _values;
  const ValueType* _valueTypes;
  const char* _separator;
  size_t _maxLineLen;
  size_t _numValues;
  const char* _header;
};

#endif //BRISTLEMOUTH_LINEPARSER_H
