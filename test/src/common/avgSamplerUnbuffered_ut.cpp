#include "gtest/gtest.h"

#include "avgSamplerUnbuffered.h"
#include "app_util.h"

// The fixture for testing class AveragingSamplerUnbuffered.
class AvgSamplerUnbufferedTest : public ::testing::Test {
protected:
  AvgSamplerUnbufferedTest() {}
  ~AvgSamplerUnbufferedTest() override {}
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AvgSamplerUnbufferedTest, normal) {
  AveragingSamplerUnbuffered sampler;
  double samples[] = {1, 1, 3, 3};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(samples[sample]));
  }
  EXPECT_DOUBLE_EQ(sampler.getMean(), 2.0);
  EXPECT_EQ(sampler.getNumSamples(), num_samples);
}

// No circular buffer: every sample counts, no wraparound / windowing.
TEST_F(AvgSamplerUnbufferedTest, accumulates_all) {
  AveragingSamplerUnbuffered sampler;
  double samples[] = {0, 0, 0, 0, 2, 2, 4, 4};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(samples[sample]));
  }
  EXPECT_DOUBLE_EQ(sampler.getMean(), 1.5);
  EXPECT_EQ(sampler.getNumSamples(), num_samples);
}

TEST_F(AvgSamplerUnbufferedTest, empty) {
  AveragingSamplerUnbuffered sampler;
  EXPECT_TRUE(isnan(sampler.getMean()) && isnan(NAN));
  EXPECT_EQ(sampler.getNumSamples(), 0);
}

TEST_F(AvgSamplerUnbufferedTest, clear) {
  AveragingSamplerUnbuffered sampler;
  double samples[] = {1.2, 2.3, 3.2, 1.2, 2.0, 2.0, 4.0, 4.0};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(samples[sample]));
  }
  sampler.clear();

  EXPECT_TRUE(isnan(sampler.getMean()) && isnan(NAN));
  EXPECT_EQ(sampler.getNumSamples(), 0);
}

TEST_F(AvgSamplerUnbufferedTest, clear_then_reuse) {
  AveragingSamplerUnbuffered sampler;
  EXPECT_TRUE(sampler.addSample(99.0));
  sampler.clear();

  EXPECT_TRUE(sampler.addSample(10.0));
  EXPECT_TRUE(sampler.addSample(20.0));
  EXPECT_DOUBLE_EQ(sampler.getMean(), 15.0);
}
