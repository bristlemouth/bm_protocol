#include "gtest/gtest.h"

#include "tokenize.h"

// The fixture for testing class Foo.
class TokenizeTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  TokenizeTest() {
     // You can do set-up work for each test here.
  }

  ~TokenizeTest() override {
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

  // Objects declared here can be used by all tests in the test suite for Foo.
};

TEST_F(TokenizeTest, BasicComma)
{
  size_t numTokens = 0;

  char testStr[] = "some,tokens,here";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 3);
  EXPECT_STREQ(tokens[0], "some");
  EXPECT_STREQ(tokens[1], "tokens");
  EXPECT_STREQ(tokens[2], "here");
  free(tokens);
}

TEST_F(TokenizeTest, BasicSpace)
{
  size_t numTokens = 0;

  char testStr[] = "some tokens here";
  char **tokens = tokenize(testStr, strlen(testStr), ' ', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 3);
  EXPECT_STREQ(tokens[0], "some");
  EXPECT_STREQ(tokens[1], "tokens");
  EXPECT_STREQ(tokens[2], "here");
  free(tokens);
}

TEST_F(TokenizeTest, NoTokens)
{
  size_t numTokens = 0;

  char testStr[] = "no tokens here";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens == NULL);
  EXPECT_EQ(numTokens, 0);
}

TEST_F(TokenizeTest, LongString)
{
  size_t numTokens = 0;

  char testStr[] = "$PUBX,00,063555.00,3723.17492,N,12205.72155,W,3.687,D3,2.7,3.0,0.042,192.54,-0.012,,0.75,1.05,0.67,14,0,0*60";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 21);
  EXPECT_STREQ(tokens[0], "$PUBX");
  EXPECT_STREQ(tokens[20], "0*60");

  free(tokens);
}

TEST_F(TokenizeTest, LongString2)
{
  size_t numTokens = 0;

  char testStr[] = "$PUBX,03,11,23,-,,,45,010,29,-,,,46,013,07,-,,,42,015,08,U,067,31,42,025,10,U,195,33,46,026,18,U,326,08,39,026,17,-,,,32,015,26,U,306,66,48,025,27,U,073,10,36,026,28,U,089,61,46,024,15,-,,,39,014*0D\r\n";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 69);
  EXPECT_STREQ(tokens[0], "$PUBX");
  EXPECT_STREQ(tokens[68], "014*0D\r\n");

  free(tokens);
}

TEST_F(TokenizeTest, EmptyString)
{
  size_t numTokens = 0;

  char testStr[] = "";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens == NULL);
  EXPECT_EQ(numTokens, 0);
}

TEST_F(TokenizeTest, JustToken)
{
  size_t numTokens = 0;

  char testStr[] = ",";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 2);
  EXPECT_TRUE(tokens[0] == NULL);

  free(tokens);
}

TEST_F(TokenizeTest, JustTokens)
{
  size_t numTokens = 0;

  char testStr[] = ",,,";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 4);
  EXPECT_TRUE(tokens[0] == NULL);
  EXPECT_TRUE(tokens[1] == NULL);
  EXPECT_TRUE(tokens[2] == NULL);
  EXPECT_TRUE(tokens[3] == NULL);

  free(tokens);
}

TEST_F(TokenizeTest, StartWithToken)
{
  size_t numTokens = 0;

  char testStr[] = ",some,tokens";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 3);
  EXPECT_TRUE(tokens[0] == NULL);
  EXPECT_STREQ(tokens[1], "some");
  EXPECT_STREQ(tokens[2], "tokens");

  free(tokens);
}

TEST_F(TokenizeTest, EndWithToken)
{
  size_t numTokens = 0;

  char testStr[] = "some,tokens,";
  char **tokens = tokenize(testStr, strlen(testStr), ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 3);
  EXPECT_STREQ(tokens[0], "some");
  EXPECT_STREQ(tokens[1], "tokens");
  EXPECT_TRUE(tokens[2] == NULL);

  free(tokens);
}

TEST_F(TokenizeTest, PartialStringOne)
{
  size_t numTokens = 0;

  char testStr[] = "0,1,2,3,4,5,6";
  char **tokens = tokenize(testStr, 4, ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 3);
  EXPECT_STREQ(tokens[0], "0");
  EXPECT_STREQ(tokens[1], "1");
  EXPECT_TRUE(tokens[2] == NULL);
  free(tokens);
}

TEST_F(TokenizeTest, PartialStringTwo)
{
  size_t numTokens = 0;

  char testStr[] = "0,1,2,3,4,5,6";
  char **tokens = tokenize(testStr, 5, ',', &numTokens);

  EXPECT_TRUE(tokens != NULL);
  EXPECT_EQ(numTokens, 3);
  EXPECT_STREQ(tokens[0], "0");
  EXPECT_STREQ(tokens[1], "1");

  // Last token is NOT zero terminated (expected)
  EXPECT_STREQ(tokens[2], "2,3,4,5,6");
  free(tokens);
}
