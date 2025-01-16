//
// Created by Evan Shapiro on 10/28/24.
//

#include "gtest/gtest.h"

#include "fff.h"
#include "string.h"
#include "OrderedSeparatorLineParser.h"
#include "Exo3LineParser.h"

extern "C" {
#include "mock_FreeRTOS.h"
}

DEFINE_FFF_GLOBALS;

static constexpr ValueType rbrParserExample[3] = {TYPE_UINT64, TYPE_DOUBLE, TYPE_DOUBLE};

// The fixture for testing class Foo.
class LineParserTest : public ::testing::Test {
protected:
  // Define the parser example configuration


  // Define the test parsers and test lines as class members
  Exo3DataLineParser exoD0Parser;
  Exo3DataLineParser exoD2Parser;
  OrderedSeparatorLineParser rbrTestParser;

  LineParserTest()
      : exoD0Parser(4, "0D0!0"),
        exoD2Parser(2, "0D2!0"),
         rbrTestParser(",", 256, rbrParserExample, 3) {
    // You can do set-up work for each test here if needed.
    exoD0Parser.init();
    exoD2Parser.init();
    rbrTestParser.init();
  }

  ~LineParserTest() override {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }
};

// Parse a single good line of RBR data
TEST_F(LineParserTest, OrderedSeparatorLineParser_Valid) {
  const char* test_line = "100, 2.5, 4.309";
  bool success = rbrTestParser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, true);
  // Retrieve and validate each value
  Value val_0 = rbrTestParser.getValue(0);
  EXPECT_EQ(val_0.type, TYPE_UINT64);
  EXPECT_EQ(val_0.data.uint64_val, 100);
  Value val_1 = rbrTestParser.getValue(1);
  EXPECT_EQ(val_1.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_1.data.double_val,2.5, 1e-6);
  Value val_2 = rbrTestParser.getValue(2);
  EXPECT_EQ(val_2.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_2.data.double_val, 4.309, 1e-6);
}

TEST_F(LineParserTest, OrderedSeparatorLineParser_FailBadFirstField) {
  const char* test_line = "a100, 2.5, 4.309";
  bool success = rbrTestParser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, false);
}

TEST_F(LineParserTest, OrderedSeparatorLineParser_FailTooFewFields) {
  const char* test_line = "100, 2.5";
  bool success = rbrTestParser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, false);
}

// Parse a single good line of EXO data, all positive.
TEST_F(LineParserTest, ExoD0Parser_Valid_AllPositive) {
  const char* test_line = "0D0!0+106.64+8.81+9.26+1.22";
  bool success = exoD0Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, true);

  // Retrieve and validate each value
  Value val_0 = exoD0Parser.getValue(0);
  EXPECT_EQ(val_0.type, TYPE_INVALID);  // Expect TYPE_INVALID for val_0

  Value val_1 = exoD0Parser.getValue(1);
  EXPECT_EQ(val_1.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_1.data.double_val, 106.64, 1e-6);

  Value val_2 = exoD0Parser.getValue(2);
  EXPECT_EQ(val_2.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_2.data.double_val, 8.81, 1e-6);

  Value val_3 = exoD0Parser.getValue(3);
  EXPECT_EQ(val_3.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_3.data.double_val, 9.26, 1e-6);

  Value val_4 = exoD0Parser.getValue(4);
  EXPECT_EQ(val_4.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_4.data.double_val, 1.22, 1e-6);
}

// Parse a single good line of EXO data, last value negative.
TEST_F(LineParserTest, ExoD0Parser_Valid_LastNegative) {
  const char* test_line = "0D0!0+25.044+7.13+6.92-9.74";
  bool success = exoD0Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, true);

  // Retrieve and validate each value
  Value val_0 = exoD0Parser.getValue(0);
  EXPECT_EQ(val_0.type, TYPE_INVALID);  // Expect TYPE_INVALID for val_0

  Value val_1 = exoD0Parser.getValue(1);
  EXPECT_EQ(val_1.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_1.data.double_val, 25.044, 1e-6);

  Value val_2 = exoD0Parser.getValue(2);
  EXPECT_EQ(val_2.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_2.data.double_val, 7.13, 1e-6);

  Value val_3 = exoD0Parser.getValue(3);
  EXPECT_EQ(val_3.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_3.data.double_val, 6.92, 1e-6);

  Value val_4 = exoD0Parser.getValue(4);
  EXPECT_EQ(val_4.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_4.data.double_val, -9.74, 1e-6);
}

// Parse a single good line of EXO data, all values negative.
TEST_F(LineParserTest, ExoD0Parser_Valid_AllNegative) {
  const char* test_line = "0D0!0-25.044-7.13-6.920-9.74-4.242";
  bool success = exoD0Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, true);

  // Retrieve and validate each value
  Value val_0 = exoD0Parser.getValue(0);
  EXPECT_EQ(val_0.type, TYPE_INVALID);  // Expect TYPE_INVALID for val_0

  Value val_1 = exoD0Parser.getValue(1);
  EXPECT_EQ(val_1.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_1.data.double_val, -25.044, 1e-6);

  Value val_2 = exoD0Parser.getValue(2);
  EXPECT_EQ(val_2.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_2.data.double_val, -7.13, 1e-6);

  Value val_3 = exoD0Parser.getValue(3);
  EXPECT_EQ(val_3.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_3.data.double_val, -6.92, 1e-6);

  Value val_4 = exoD0Parser.getValue(4);
  EXPECT_EQ(val_4.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_4.data.double_val, -9.74, 1e-6);
}

// Parse a single good line of EXO data, 2 values.
TEST_F(LineParserTest, ExoD0Parser_Valid_2Values) {
  const char* test_line = "0D2!0-0.437+12.12";
  bool success = exoD2Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, true);

  // Retrieve and validate each value
  Value val_0 = exoD2Parser.getValue(0);
  EXPECT_EQ(val_0.type, TYPE_INVALID);  // Expect TYPE_INVALID for val_0

  Value val_1 = exoD2Parser.getValue(1);
  EXPECT_EQ(val_1.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_1.data.double_val, -0.437, 1e-6);

  Value val_2 = exoD2Parser.getValue(2);
  EXPECT_EQ(val_2.type, TYPE_DOUBLE);
  EXPECT_NEAR(val_2.data.double_val, 12.12, 1e-6);
}

TEST_F(LineParserTest, ExoD0Parser_FailTooFewValues) {
  const char* test_line = "0D0!0+0.437+12.12";
  bool success = exoD0Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, false);
}

TEST_F(LineParserTest, ExoD0Parser_FailBadSeparator) {
  const char* test_line = "0D0!0+25.044,7.13+6.92-9.74";
  bool success = exoD0Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, false);
}

TEST_F(LineParserTest, ExoD0Parser_FailWrongResponseHeader) {
  const char* test_line = "0D1!0+25.044,7.13+6.92-9.74";
  bool success = exoD0Parser.parseLine(test_line, strlen(test_line));
  EXPECT_EQ(success, false);
}
