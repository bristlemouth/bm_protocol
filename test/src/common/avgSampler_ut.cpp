#include "gtest/gtest.h"

#include "avgSampler.h"
#include "app_util.h"

// The fixture for testing class Foo.
class AvgSamplerTest : public ::testing::Test {
protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  AvgSamplerTest() {
    // You can do set-up work for each test here.
  }

  ~AvgSamplerTest() override {
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

TEST_F(AvgSamplerTest, normal) {
  AveragingSampler sampler;
  sampler.initBuffer(4);
  double samples[] = {1, 1, 3, 3};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint16_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(samples[sample], (sample + 1)));
  }
  EXPECT_DOUBLE_EQ(sampler.getMean(), 2.0);
  EXPECT_DOUBLE_EQ(sampler.getVariance(), 1.0);
  EXPECT_DOUBLE_EQ(sampler.getStd(), sqrt(1.0));
  EXPECT_DOUBLE_EQ(sampler.getMax(), 3.0);
  EXPECT_DOUBLE_EQ(sampler.getMin(), 1.0);
}

TEST_F(AvgSamplerTest, partial) {
  // Make buffer larger than number of samples
  AveragingSampler sampler;
  sampler.initBuffer(16);
  double samples[] = {1, 1, 3, 3};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(samples[sample], (sample + 1)));
  }
  EXPECT_DOUBLE_EQ(sampler.getMean(), 2.0);
  EXPECT_DOUBLE_EQ(sampler.getVariance(), 1.0);
  EXPECT_DOUBLE_EQ(sampler.getStd(), sqrt(1.0));
  EXPECT_DOUBLE_EQ(sampler.getMax(), 3.0);
  EXPECT_DOUBLE_EQ(sampler.getMin(), 1.0);
}

TEST_F(AvgSamplerTest, wraparound) {
  AveragingSampler sampler;
  sampler.initBuffer(4);
  double samples[] = {0, 0, 0, 0, 2, 2, 4, 4};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(samples[sample], (sample + 1)));
  }
  EXPECT_DOUBLE_EQ(sampler.getMean(), 3.0);
  EXPECT_DOUBLE_EQ(sampler.getMax(), 4.0);
  EXPECT_DOUBLE_EQ(sampler.getMin(), 2.0);
  EXPECT_DOUBLE_EQ(sampler.getVariance(), 1.0);
  EXPECT_DOUBLE_EQ(sampler.getStd(), sqrt(1.0));
}

TEST_F(AvgSamplerTest, empty) {
  AveragingSampler sampler;
  sampler.initBuffer(4);
  EXPECT_TRUE(isnan(sampler.getMean()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getVariance()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getStd()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getMin()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getMax()) && isnan(NAN));
}

TEST_F(AvgSamplerTest, baro_256) {
  double samples[] = {
      1019.64, 1019.64, 1019.59, 1019.59, 1019.59, 1019.52, 1019.62, 1019.62, 1019.67, 1019.65,
      1019.61, 1019.65, 1019.57, 1019.57, 1019.60, 1019.63, 1019.65, 1019.63, 1019.62, 1019.61,
      1019.63, 1019.66, 1019.61, 1019.64, 1019.62, 1019.63, 1019.57, 1019.61, 1019.63, 1019.62,
      1019.64, 1019.63, 1019.62, 1019.61, 1019.60, 1019.66, 1019.61, 1019.58, 1019.65, 1019.58,
      1019.61, 1019.62, 1019.62, 1019.66, 1019.67, 1019.60, 1019.64, 1019.66, 1019.61, 1019.61,
      1019.60, 1019.61, 1019.60, 1019.61, 1019.62, 1019.66, 1019.61, 1019.65, 1019.68, 1019.64,
      1019.67, 1019.63, 1019.64, 1019.62, 1019.64, 1019.62, 1019.65, 1019.65, 1019.62, 1019.68,
      1019.60, 1019.65, 1019.61, 1019.60, 1019.63, 1019.65, 1019.65, 1019.59, 1019.61, 1019.59,
      1019.67, 1019.66, 1019.61, 1019.63, 1019.61, 1019.71, 1019.59, 1019.65, 1019.62, 1019.69,
      1019.60, 1019.63, 1019.67, 1019.68, 1019.62, 1019.66, 1019.69, 1019.64, 1019.65, 1019.65,
      1019.62, 1019.65, 1019.63, 1019.62, 1019.60, 1019.55, 1019.60, 1019.65, 1019.62, 1019.64,
      1019.69, 1019.61, 1019.63, 1019.62, 1019.63, 1019.61, 1019.63, 1019.70, 1019.65, 1019.67,
      1019.65, 1019.69, 1019.65, 1019.70, 1019.64, 1019.64, 1019.64, 1019.65, 1019.61, 1019.70,
      1019.68, 1019.66, 1019.63, 1019.61, 1019.61, 1019.61, 1019.65, 1019.67, 1019.63, 1019.61,
      1019.62, 1019.63, 1019.65, 1019.62, 1019.68, 1019.66, 1019.66, 1019.63, 1019.61, 1019.63,
      1019.60, 1019.57, 1019.62, 1019.66, 1019.60, 1019.63, 1019.62, 1019.62, 1019.63, 1019.59,
      1019.56, 1019.65, 1019.63, 1019.64, 1019.62, 1019.70, 1019.61, 1019.66, 1019.62, 1019.61,
      1019.63, 1019.66, 1019.66, 1019.63, 1019.67, 1019.60, 1019.59, 1019.62, 1019.65, 1019.61,
      1019.67, 1019.61, 1019.66, 1019.65, 1019.63, 1019.66, 1019.60, 1019.62, 1019.65, 1019.63,
      1019.60, 1019.66, 1019.57, 1019.60, 1019.60, 1019.60, 1019.63, 1019.59, 1019.66, 1019.62,
      1019.62, 1019.63, 1019.61, 1019.58, 1019.60, 1019.58, 1019.59, 1019.61, 1019.62, 1019.59,
      1019.63, 1019.57, 1019.62, 1019.58, 1019.63, 1019.64, 1019.59, 1019.58, 1019.64, 1019.59,
      1019.61, 1019.61, 1019.62, 1019.63, 1019.62, 1019.61, 1019.61, 1019.61, 1019.64, 1019.64,
      1019.66, 1019.63, 1019.59, 1019.63, 1019.65, 1019.61, 1019.63, 1019.67, 1019.63, 1019.63,
      1019.66, 1019.66, 1019.64, 1019.58, 1019.61, 1019.59, 1019.57, 1019.61, 1019.64, 1019.61,
      1019.65, 1019.62, 1019.64, 1019.59, 1019.62, 1019.60};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);
  AveragingSampler sampler;
  sampler.initBuffer(num_samples);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(samples[sample], (sample + 1)));
  }

  // value computed with numpy.float64() to match the double quirks
  EXPECT_NEAR(sampler.getMean(), 1019.6269921874995, 0.0003);
  EXPECT_NEAR(sampler.getMean(true), 1019.6269921875, 0.0003);
  EXPECT_NEAR(sampler.getVariance(), 0.0008874374389640093, 0.0003);
  EXPECT_NEAR(sampler.getStd(), 0.02978988819992464, 0.0003);
  EXPECT_DOUBLE_EQ(sampler.getMax(), 1019.71);
  EXPECT_DOUBLE_EQ(sampler.getMin(), 1019.52);
}

TEST_F(AvgSamplerTest, clear) {
  AveragingSampler sampler;
  sampler.initBuffer(4);
  double samples[] = {1.2, 2.3, 3.2, 1.2, 2.0, 2.0, 4.0, 4.0};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(samples[sample], (sample + 1)));
  }

  sampler.clear();

  EXPECT_TRUE(isnan(sampler.getMean()) && isnan(NAN));
}

TEST_F(AvgSamplerTest, clear2) {
  AveragingSampler sampler;
  sampler.initBuffer(4);
  double samples[] = {1.2, 2.3, 3.2, 1.2, 2.0, 2.0, 4.0, 4.0};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(samples[sample], (sample + 1)));
  }

  sampler.clear();

  EXPECT_TRUE(sampler.addSampleTimestamped(10.0));
  EXPECT_TRUE(sampler.addSampleTimestamped(20.0));

  EXPECT_DOUBLE_EQ(sampler.getMean(), 15.0);
}

TEST_F(AvgSamplerTest, timestamp_test) {
  AveragingSampler sampler;
  sampler.initBuffer(4);

  EXPECT_TRUE(sampler.addSampleTimestamped(1.0, 1));

  // Don't allow same-timestamp as previous value
  EXPECT_FALSE(sampler.addSampleTimestamped(2.0, 1));

  // Allow since timestamp has changed
  EXPECT_TRUE(sampler.addSampleTimestamped(3.0, 2));

  // Always allow if no timestamp is present
  EXPECT_TRUE(sampler.addSampleTimestamped(4.0));

  EXPECT_EQ(sampler.getNumSamples(), 3);
}


TEST_F(AvgSamplerTest, circ) {
  AveragingSampler sampler;
  sampler.initBuffer(4);
  double samplesDeg[] = {90, 120, 33, 355};
  const uint32_t num_samples = sizeof(samplesDeg) / sizeof(double);

  for (uint16_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp for now
    EXPECT_TRUE(sampler.addSampleTimestamped(degToRad(samplesDeg[sample]), (sample + 1)));
  }
  // Tested against custom python impl:
  // The wave std (in radians) is:
  // np.sqrt(2 - 2 * np.sqrt(a1 ** 2 + b1 ** 2)
  // where a1 is the mean of the cosine of the angles, and b1 is the mean of the sine of the angles.
  EXPECT_NEAR(sampler.getCircularStd(),  0.8125, 0.0001);
  EXPECT_NEAR(sampler.getCircularMean(), 1.0493, 0.0001); // Tested vs https://docs.scipy.org/doc/scipy/reference/generated/scipy.stats.circmean.html
}

// See the discussion here for context on why this test was written
// https://github.com/bristlemouth/bm_protocol/pull/42
TEST_F(AvgSamplerTest, circ_mean_is_positive) {
  AveragingSampler sampler;
  sampler.initBuffer(10);
  double samplesDeg[] = {232.203, 227.507, 317.123, 317.778, 226.359,
                         118.473, 295.796, 241.099, 133.762, 205.290};
  const size_t num_samples = sizeof(samplesDeg) / sizeof(double);

  for (size_t sample = 0; sample < num_samples; sample++) {
    // Use sample+1 as timestamp
    EXPECT_TRUE(sampler.addSampleTimestamped(degToRad(samplesDeg[sample]), (sample + 1)));
  }

  EXPECT_NEAR(sampler.getCircularMean(), 4.1542, 0.0001);
}
