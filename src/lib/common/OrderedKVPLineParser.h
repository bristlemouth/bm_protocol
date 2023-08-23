//
// Created by Evan Shapiro on 8/17/23.
//

#ifndef SPOTTER_ORDEREDKVPLINEPARSER_H
#define SPOTTER_ORDEREDKVPLINEPARSER_H

#include "LineParser.h"

class OrderedKVPLineParser : public LineParser {
public:
  OrderedKVPLineParser(const char* separator, size_t maxLineLen, ValueType* valueTypes, size_t numValues,
                             const char** keys, const char* header = nullptr);

private:
  // Implementation for parsing a value of given index from the line
  bool parseValues(char* workStr) override;
  const char** _keys;

};


#endif //SPOTTER_ORDEREDKVPLINEPARSER_H
