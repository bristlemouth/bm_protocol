#include "gtest/gtest.h"

#include "avgSamplerCircular.h"
#include "app_util.h"

// The fixture for testing class AveragingSamplerCircular.
class AvgSamplerCircularTest : public ::testing::Test {
protected:
  AvgSamplerCircularTest() {}
  ~AvgSamplerCircularTest() override {}
  void SetUp() override {}
  void TearDown() override {}
};

// Same inputs/expected as AvgSamplerTest.circ (buffered).
TEST_F(AvgSamplerCircularTest, circ) {
  AveragingSamplerCircular sampler;
  double samplesDeg[] = {90, 120, 33, 355};
  const uint32_t num_samples = sizeof(samplesDeg) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(degToRad(samplesDeg[sample])));
  }
  // The wave std (in radians) is:
  // np.sqrt(2 - 2 * np.sqrt(a1 ** 2 + b1 ** 2))
  // where a1 is the mean of the cosine of the angles, b1 the mean of the sine.
  EXPECT_NEAR(sampler.getCircularStd(), 0.8125, 0.0001);
  EXPECT_NEAR(sampler.getCircularMean(), 1.0493, 0.0001);
}

// See https://github.com/bristlemouth/bm_protocol/pull/42 -- mean must be >= 0.
TEST_F(AvgSamplerCircularTest, circ_mean_is_positive) {
  AveragingSamplerCircular sampler;
  double samplesDeg[] = {232.203, 227.507, 317.123, 317.778, 226.359,
                         118.473, 295.796, 241.099, 133.762, 205.290};
  const size_t num_samples = sizeof(samplesDeg) / sizeof(double);

  for (size_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(degToRad(samplesDeg[sample])));
  }
  EXPECT_NEAR(sampler.getCircularMean(), 4.1542, 0.0001);
}

TEST_F(AvgSamplerCircularTest, empty) {
  AveragingSamplerCircular sampler;
  EXPECT_TRUE(isnan(sampler.getCircularMean()) && isnan(NAN));
  EXPECT_TRUE(isnan(sampler.getCircularStd()) && isnan(NAN));
  EXPECT_EQ(sampler.getNumSamples(), 0);
}

TEST_F(AvgSamplerCircularTest, clear) {
  AveragingSamplerCircular sampler;
  double samplesDeg[] = {90, 120, 33, 355};
  const uint32_t num_samples = sizeof(samplesDeg) / sizeof(double);

  for (uint32_t sample = 0; sample < num_samples; sample++) {
    EXPECT_TRUE(sampler.addSample(degToRad(samplesDeg[sample])));
  }
  sampler.clear();

  EXPECT_TRUE(isnan(sampler.getCircularMean()) && isnan(NAN));
  EXPECT_EQ(sampler.getNumSamples(), 0);
}
