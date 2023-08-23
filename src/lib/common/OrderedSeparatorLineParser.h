//
// Created by Evan Shapiro on 8/17/23.
//

#ifndef SPOTTER_ORDEREDSEPARATORLINEPARSER_H
#define SPOTTER_ORDEREDSEPARATORLINEPARSER_H

#include "LineParser.h"

class OrderedSeparatorLineParser : public LineParser {
public:
  OrderedSeparatorLineParser(const char* separator, size_t maxLineLen, ValueType* valueTypes, size_t numValues,
                             const char* header = nullptr);

private:
  // Implementation for parsing a value of given index from the line
  bool parseValues(char* workStr) override;

};


#endif //SPOTTER_ORDEREDSEPARATORLINEPARSER_H
