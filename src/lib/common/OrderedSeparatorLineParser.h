//OrderedSeparatorLineParser.h
// Created by Evan Shapiro on 8/17/23.
//
// OrderedSeparatorLineParser provides line parsing functionality with custom separator support.
// Parses structured data lines where fields are separated by a specified character.
// This class is derived from LineParser and supports flexible field separation for data processing.


#ifndef SPOTTER_ORDEREDSEPARATORLINEPARSER_H
#define SPOTTER_ORDEREDSEPARATORLINEPARSER_H

#include "LineParser.h"

/**
 * @class OrderedSeparatorLineParser
 * @brief A parser for data lines with fixed number of ordered fields,
 * separated by a character from a specific set of characters.
 *
 * OrderedSeparatorLineParser interprets lines where each field is separated by a single
 * separator character. It supports parsing fields based on the specified separator,
 * with flexibility for handling different data types in each field.
 *
 * Some serial schemas may use a set of separator characters - for example ['+', '-'] denoting
 * both separation and sign. In this case, the set can be passed into the separator field of the
 * constructor as a string "+-", and the parser will separate values on the
 * first detected character from the set.
 * It WILL NOT separate on multi-character separators.
 *
 */
class OrderedSeparatorLineParser : public LineParser {
public:
  /**
 * Constructs an OrderedSeparatorLineParser.
 *
 * @param separator A string containing the set of characters used to separate fields in the data line.
 *                  Each character in this string is treated as a possible separator, allowing
 *                  the parser to split on the first occurrence of any character in the set.
 * @param maxLineLen The maximum length of a line to parse. Lines exceeding this length will cause overruns.
 * @param valueTypes An array specifying the expected ValueType for each field in the data line.
 * @param numValues The fixed number of fields to parse from each line.
 * @param header Optional string representing a prefix or header for identifying valid data lines.
 */
  OrderedSeparatorLineParser(const char* separator, size_t maxLineLen, const ValueType* valueTypes, size_t numValues,
                             const char* header = nullptr);

private:
  /**
 * Parses values from the line based on the separator set, storing results in the parser's _values array.
 * The base class function LineParser::parseLine creates a mutable copy of the line, and calls
 * this function passing that as the workStr, then deletes the copy when done.
 *
 * @param workStr A mutable copy of the data line to parse; this string will be modified in place
 *                as it is split on the first occurrence of any character from the separator set.
 * @return True if all expected values are successfully parsed; false otherwise.
 */
  bool parseValues(char* workStr) override;

};


#endif //SPOTTER_ORDEREDSEPARATORLINEPARSER_H
