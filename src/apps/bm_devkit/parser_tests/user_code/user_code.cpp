// Unit test application for LineParser classes, specifically OrderedSeparatorLineParser
// and Exo3DataLineParser, which parse structured data lines with customizable separators.
//
// This file configures and tests parsers with:
// - Sample RBR and Exo3 data lines that simulate valid, edge-case, and invalid formats.
// - Initialization and assertions to verify each parser’s handling of expected values and errors.
//

#include "user_code.h"
#include "string.h"
#include "OrderedSeparatorLineParser.h"
#include "Exo3LineParser.h"

#define LED_ON_TIME_MS 20
#define LED_PERIOD_MS 1000

static constexpr ValueType rbrParserExample[] = {TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE};

OrderedSeparatorLineParser rbrTestParser(",", 256, rbrParserExample, 3);

const char* RBR_TEST_LINES[] = {
  "100, 2.5, 4.309",
  // Bad value field
  "a100, 2.5, 4.309",
};

const char* EXO_D0_TEST_LINES[] = {
  "0D0!0+106.64+8.81+9.26+1.22",
  "0D0!0+25.044+7.13+6.92-9.74",
  // All negative values, extra value at end ignored
  "0D0!0-25.044-7.13-6.920-9.74-4.242",
  // Too few values
  "0D0!0+0.437+12.12",
  // Bad separator
  "0D0!0+25.044,7.13+6.92-9.74",
  // Wrong response header
  "0D1!0+25.044,7.13+6.92-9.74"
};

const char* EXO_D2_TEST_LINES[] = {
// 2 value line
  "0D2!0-0.437+12.12",
};

Exo3DataLineParser exoD0Parser(4, "0D0!0");
Exo3DataLineParser exoD2Parser(2, "0D2!0");

void setup(void) {
  /* USER ONE-TIME SETUP CODE GOES HERE */
  vTaskDelay(1000);
  printf("Testing line parsers...\n\n");
  rbrTestParser.init();
  exoD0Parser.init();
  exoD2Parser.init();

  printf("parsing: %s\n", RBR_TEST_LINES[0]);
  bool success = rbrTestParser.parseLine(RBR_TEST_LINES[0], strlen(RBR_TEST_LINES[0]));
  configASSERT(success == true);

  // Retrieve and validate each value
  Value val_0 = rbrTestParser.getValue(0);
  configASSERT(val_0.type == TYPE_UINT64 && val_0.data.uint64_val == 100);
  printf("Value 0 is UINT64: %llu\n", val_0.data.uint64_val);

  Value val_1 = rbrTestParser.getValue(1);
  configASSERT(val_1.type == TYPE_DOUBLE && fabs(val_1.data.double_val - 2.5) < 1e-6);
  printf("Value 1 is DOUBLE: %.2f\n", val_1.data.double_val);

  Value val_2 = rbrTestParser.getValue(2);
  configASSERT(val_2.type == TYPE_DOUBLE && fabs(val_2.data.double_val - 4.309) < 1e-6);
  printf("Value 2 is DOUBLE: %.3f\n", val_2.data.double_val);

  printf("All RBR values parsed and verified successfully.\n\n");

  printf("parsing: %s\n", EXO_D0_TEST_LINES[0]);
  success = exoD0Parser.parseLine(EXO_D0_TEST_LINES[0], strlen(EXO_D0_TEST_LINES[0]));
  configASSERT(success == true);

  // Retrieve and validate each value
  val_0 = exoD0Parser.getValue(0);
  configASSERT(val_0.type == TYPE_INVALID);
  printf("Value 0 is TYPE_INVALID\n");

  val_1 = exoD0Parser.getValue(1);
  configASSERT(val_1.type == TYPE_DOUBLE && fabs(val_1.data.double_val - 106.64) < 1e-6);
  printf("Value 1 is DOUBLE: %.2f\n", val_1.data.double_val);

  val_2 = exoD0Parser.getValue(2);
  configASSERT(val_2.type == TYPE_DOUBLE && fabs(val_2.data.double_val - 8.81) < 1e-6);
  printf("Value 2 is DOUBLE: %.3f\n", val_2.data.double_val);

  Value val_3 = exoD0Parser.getValue(3);
  configASSERT(val_3.type == TYPE_DOUBLE && fabs(val_3.data.double_val - 9.26) < 1e-6);
  printf("Value 3 is DOUBLE: %.3f\n", val_3.data.double_val);

  Value val_4 = exoD0Parser.getValue(4);
  configASSERT(val_4.type == TYPE_DOUBLE && fabs(val_4.data.double_val - 1.22) < 1e-6);
  printf("Value 4 is DOUBLE: %.3f\n", val_4.data.double_val);

  printf("parsing: %s\n", EXO_D0_TEST_LINES[1]);
  success = exoD0Parser.parseLine(EXO_D0_TEST_LINES[1], strlen(EXO_D0_TEST_LINES[1]));
  configASSERT(success == true);

  val_0 = exoD0Parser.getValue(0);
  configASSERT(val_0.type == TYPE_INVALID);
  printf("Value 0 is TYPE_INVALID\n");

  val_1 = exoD0Parser.getValue(1);
  configASSERT(val_1.type == TYPE_DOUBLE && fabs(val_1.data.double_val - 25.044) < 1e-6);
  printf("Value 1 is DOUBLE: %.2f\n", val_1.data.double_val);

  val_2 = exoD0Parser.getValue(2);
  configASSERT(val_2.type == TYPE_DOUBLE && fabs(val_2.data.double_val - 7.13) < 1e-6);
  printf("Value 2 is DOUBLE: %.3f\n", val_2.data.double_val);

  val_3 = exoD0Parser.getValue(3);
  configASSERT(val_3.type == TYPE_DOUBLE && fabs(val_3.data.double_val - 6.92) < 1e-6);
  printf("Value 3 is DOUBLE: %.3f\n", val_3.data.double_val);

  val_4 = exoD0Parser.getValue(4);
  configASSERT(val_4.type == TYPE_DOUBLE && fabs(val_4.data.double_val + 9.74) < 1e-6);
  printf("Value 4 is DOUBLE: %.3f\n", val_4.data.double_val);

  printf("parsing: %s\n", EXO_D0_TEST_LINES[2]);
  success = exoD0Parser.parseLine(EXO_D0_TEST_LINES[2], strlen(EXO_D0_TEST_LINES[2]));
  configASSERT(success == true);

  val_0 = exoD0Parser.getValue(0);
  configASSERT(val_0.type == TYPE_INVALID);
  printf("Value 0 is TYPE_INVALID\n");

  val_1 = exoD0Parser.getValue(1);
  configASSERT(val_1.type == TYPE_DOUBLE && fabs(val_1.data.double_val + 25.044) < 1e-6);
  printf("Value 1 is DOUBLE: %.2f\n", val_1.data.double_val);

  val_2 = exoD0Parser.getValue(2);
  configASSERT(val_2.type == TYPE_DOUBLE && fabs(val_2.data.double_val + 7.13) < 1e-6);
  printf("Value 2 is DOUBLE: %.3f\n", val_2.data.double_val);

  val_3 = exoD0Parser.getValue(3);
  configASSERT(val_3.type == TYPE_DOUBLE && fabs(val_3.data.double_val + 6.92) < 1e-6);
  printf("Value 3 is DOUBLE: %.3f\n", val_3.data.double_val);

  val_4 = exoD0Parser.getValue(4);
  configASSERT(val_4.type == TYPE_DOUBLE && fabs(val_4.data.double_val + 9.74) < 1e-6);
  printf("Value 4 is DOUBLE: %.3f\n", val_4.data.double_val);

  printf("parsing: %s\n", EXO_D2_TEST_LINES[0]);
  success = exoD2Parser.parseLine(EXO_D2_TEST_LINES[0], strlen(EXO_D2_TEST_LINES[0]));
  configASSERT(success == true);

  val_0 = exoD2Parser.getValue(0);
  configASSERT(val_0.type == TYPE_INVALID);
  printf("Value 0 is TYPE_INVALID\n");

  val_1 = exoD2Parser.getValue(1);
  configASSERT(val_1.type == TYPE_DOUBLE && fabs(val_1.data.double_val + 0.437) < 1e-6);
  printf("Value 1 is DOUBLE: %.2f\n", val_1.data.double_val);

  val_2 = exoD2Parser.getValue(2);
  configASSERT(val_2.type == TYPE_DOUBLE && fabs(val_2.data.double_val - 12.12) < 1e-6);
  printf("Value 2 is DOUBLE: %.3f\n", val_2.data.double_val);
  printf("All good EXO values parsed and verified successfully.\n\n");

  printf("Testing fail cases\n");
  printf("parsing: %s\n", EXO_D0_TEST_LINES[3]);
  success = exoD0Parser.parseLine(EXO_D0_TEST_LINES[3], strlen(EXO_D0_TEST_LINES[3]));
  configASSERT(success == false);
  printf("parsing: %s\n", EXO_D0_TEST_LINES[4]);
  success = exoD0Parser.parseLine(EXO_D0_TEST_LINES[4], strlen(EXO_D0_TEST_LINES[4]));
  configASSERT(success == false);
  printf("parsing: %s\n", EXO_D0_TEST_LINES[5]);
  success = exoD0Parser.parseLine(EXO_D0_TEST_LINES[5], strlen(EXO_D0_TEST_LINES[5]));
  configASSERT(success == false);
  printf("parsing: %s\n", RBR_TEST_LINES[1]);
  success = rbrTestParser.parseLine(RBR_TEST_LINES[1], strlen(RBR_TEST_LINES[1]));
  configASSERT(success == false);

  printf("\nALL TESTS COMPLETED SUCCESSFULLY.\n");
}

void loop(void) {}
