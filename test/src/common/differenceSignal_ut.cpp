#include "gtest/gtest.h"

#include "differenceSignal.h"
#include "fff.h"
#include "app_util.h"

extern "C" {
#include "mock_FreeRTOS.h"
}

DEFINE_FFF_GLOBALS;

// The fixture for testing class Foo.
class DifferenceSignalTest : public ::testing::Test {
protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  DifferenceSignalTest() {
    // You can do set-up work for each test here.
  }

  ~DifferenceSignalTest() override {
    // You can do clean-up work that doesn't throw exceptions here.
  }

  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
    RESET_FAKE(xQueueCreateMutex);
    RESET_FAKE(xQueueSemaphoreTake);
    xQueueCreateMutex_fake.return_val = (QueueHandle_t)0x1234;
    xQueueSemaphoreTake_fake.return_val = pdTRUE;
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Objects declared here can be used by all tests in the test suite for Foo.
};

TEST_F(DifferenceSignalTest, Normal) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  const double key[] = {198.7, -177.7, 64.5, -78.4};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  double d_n[5];
  size_t size = 5;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key[i], 0.00001);
  }
  EXPECT_EQ(size, 4);
  EXPECT_TRUE(ds.isFull());
  double r0;
  EXPECT_EQ(ds.getReferenceSignal(r0), true);
  EXPECT_NEAR(r0, 1015.6, 0.00001);

  // 2nd round of samples
  ds.clear();
  EXPECT_FALSE(ds.isFull());
  const double samples2[] = {12334.2, 14375.6, 11375.0, 15345.1, 12338.3};
  const double key2[] = {2041.4, -3000.6, 3970.1, -3006.8};
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples2[sample]));
  }
  size = 5;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key2[i], 0.00001);
  }
  EXPECT_EQ(size, 4);
  EXPECT_EQ(ds.getReferenceSignal(r0), true);
  EXPECT_NEAR(r0, 12334.2, 0.00001);
}

TEST_F(DifferenceSignalTest, Empty) {
  DifferenceSignal ds(5);
  double d_n[5];
  size_t size = 5;
  EXPECT_FALSE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  EXPECT_EQ(size, 0);
}

TEST_F(DifferenceSignalTest, HalfFull) {
  const double samples[] = {1015.6, 1214.3, 1036.6};
  const double key[] = {198.7, -177.7};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 3; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  double d_n[5];
  size_t size = 5;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key[i], 0.00001);
  }
  EXPECT_EQ(size, 2);
  EXPECT_FALSE(ds.isFull());
}

TEST_F(DifferenceSignalTest, Full) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  const double key[] = {198.7, -177.7, 64.5, -78.4};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  EXPECT_FALSE(ds.addSample(0.0));
  double d_n[5];
  size_t size = 5;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key[i], 0.00001);
  }
  EXPECT_EQ(size, 4);
  EXPECT_TRUE(ds.isFull());
}

TEST_F(DifferenceSignalTest, BadEncode) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  double d_n[5];
  size_t size = 0;
  EXPECT_DEATH(ds.encodeDifferenceSignalToBuffer(NULL, size), "");
  EXPECT_DEATH(ds.encodeDifferenceSignalToBuffer(d_n, size), "");
}

TEST_F(DifferenceSignalTest, SmallBuffer) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  const double key[] = {198.7};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  double d_n[1];
  size_t size = 1;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  EXPECT_EQ(size, 1);
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key[i], 0.00001);
  }
}

TEST_F(DifferenceSignalTest, SmallBuffer2) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  const double key[] = {198.7, -177.7, 64.5, -78.4};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  double d_n[4];
  size_t size = 4;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  EXPECT_EQ(size, 4);
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key[i], 0.00001);
  }
}

TEST_F(DifferenceSignalTest, BadInit) { EXPECT_DEATH(DifferenceSignal ds(0), ""); }

TEST_F(DifferenceSignalTest, signalMean) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  EXPECT_NEAR(ds.signalMean(), 1078.06, 0.00001);
}

TEST_F(DifferenceSignalTest, orderTwoSignal) {
  const double samples[] = {1015.6, 1214.3, 1036.6, 1101.1, 1022.7};
  const double key[] = {198.7, -177.7, 64.5, -78.4};
  DifferenceSignal ds(5);
  for (uint32_t sample = 0; sample < 5; sample++) {
    EXPECT_TRUE(ds.addSample(samples[sample]));
  }
  double d_n[5];
  size_t size = 5;
  EXPECT_TRUE(ds.encodeDifferenceSignalToBuffer(d_n, size));
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key[i], 0.00001);
  }
  EXPECT_EQ(size, 4);
  EXPECT_TRUE(ds.isFull());
  double r0;
  EXPECT_EQ(ds.getReferenceSignal(r0), true);
  EXPECT_NEAR(r0, 1015.6, 0.00001);

  // 2nd Order Signal
  const double key2[] = {-376.4, 242.2, -142.9};
  EXPECT_TRUE(DifferenceSignal::differenceSignalFromBuffer(d_n, size, r0));
  EXPECT_EQ(size, 3);
  EXPECT_NEAR(r0, 198.7, 0.00001);
  for (uint32_t i = 0; i < size; i++) {
    EXPECT_NEAR(d_n[i], key2[i], 0.00001);
  }
}
