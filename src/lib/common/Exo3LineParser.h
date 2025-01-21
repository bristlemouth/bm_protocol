// Exo3DataLineParser.h
//
// A parser for Exo3 data lines with custom separator handling.
// Created by Evan Shapiro on 10/25/24.
//
// Exo3 data lines have a command-response identifier as the first field,
// followed by numeric data fields separated by '+' or '-' characters.
// The '+' and '-' separators also indicate positive or negative values,
// requiring custom parsing that uses the separator to interpret each field's sign and value.
// Example line: "0D0!0-25.044-7.13-6.920-9.74-4.242"

#ifndef EXO3DATALINEPARSER_H
#define EXO3DATALINEPARSER_H

#include "OrderedSeparatorLineParser.h"

class Exo3DataLineParser : public OrderedSeparatorLineParser {
public:
  /**
 * Constructor for Exo3DataLineParser.
 *
 * @param numValues The number of expected values in the line to parse,
 *  excluding the response identifier header.
 * @param header Header string to specify the expected response identifier.
 */
  Exo3DataLineParser(size_t numValues, const char* header);
  ~Exo3DataLineParser() override;

private:
  /**
 * Parses a token from the line, interpreting '+' or '-' as both delimiter and sign.
 *
 * @param token The string token to parse the value from, null-terminated.
 * @param index The index in the _values array to store the parsed value.
 * @param foundSeparator The separator found before this token, indicating sign.
 * @return True if the token was successfully parsed; false otherwise.
 */
  bool parseValueFromToken(const char* token, size_t index, char foundSeparator) override;

};


#endif //EXO3DATALINEPARSER_H
