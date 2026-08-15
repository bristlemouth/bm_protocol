#include "gtest/gtest.h"

#include "avgSamplerStats.h"
#include "app_util.h"

// The fixture for testing class AveragingSamplerStats.
class AvgSamplerStatsTest : public ::testing::Test {
protected:
  AvgSamplerStatsTest() {}
  ~AvgSamplerStatsTest() override {}
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(AvgSamplerStatsTest, normal) {
  AveragingSamplerStats sampler;
  double samples[] = {1, 1, 3, 3};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(samples[sample]));
  }
  EXPECT_DOUBLE_EQ(sampler.getMean(), 2.0);
  EXPECT_DOUBLE_EQ(sampler.getVariance(), 1.0);
  EXPECT_DOUBLE_EQ(sampler.getStd(), sqrt(1.0));
  EXPECT_DOUBLE_EQ(sampler.getMax(), 3.0);
  EXPECT_DOUBLE_EQ(sampler.getMin(), 1.0);
  EXPECT_EQ(sampler.getNumSamples(), num_samples);
}

// Same dataset and expected values as AvgSamplerTest.baro_256 (buffered),
// proving the streaming stats match the buffered implementation.
TEST_F(AvgSamplerStatsTest, baro_256) {
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
      1019.65, 1019.62, 1019.64, 1019.59, 1019.62, 1019.60 };
  const uint32_t num_samples = sizeof(samples) / sizeof(double);
  AveragingSamplerStats sampler;

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(samples[sample]));
  }

  // values computed with numpy.float64() to match the double quirks
  EXPECT_NEAR(sampler.getMean(), 1019.6269921875, 0.0003);
  EXPECT_NEAR(sampler.getVariance(), 0.0008874374389640093, 0.0003);
  EXPECT_NEAR(sampler.getStd(), 0.02978988819992464, 0.0003);
  EXPECT_DOUBLE_EQ(sampler.getMax(), 1019.71);
  EXPECT_DOUBLE_EQ(sampler.getMin(), 1019.52);
}

TEST_F(AvgSamplerStatsTest, empty) {
  AveragingSamplerStats sampler;
  EXPECT_TRUE(isnan(sampler.getMean()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getVariance()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getStd()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getMin()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getMax()) && isnan(NAN));
}

TEST_F(AvgSamplerStatsTest, clear) {
  AveragingSamplerStats sampler;
  double samples[] = {1.2, 2.3, 3.2, 1.2, 2.0, 2.0, 4.0, 4.0};
  const uint32_t num_samples = sizeof(samples) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(samples[sample]));
  }
  sampler.clear();

  EXPECT_TRUE(isnan(sampler.getMean()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getMin()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getMax()) && isnan(NAN));
  EXPECT_EQ(sampler.getNumSamples(), 0);

  EXPECT_TRUE(sampler.addSample(5.0));
  EXPECT_TRUE(sampler.addSample(7.0));
  EXPECT_DOUBLE_EQ(sampler.getMin(), 5.0);
  EXPECT_DOUBLE_EQ(sampler.getMax(), 7.0);
}
