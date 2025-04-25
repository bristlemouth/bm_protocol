#include "spectral_entropy.h"
#include <math.h>

/**
 * @brief Computes the normalized spectral entropy.
 *
 * Converts an array of SPL values (in dB) to a linear scale,
 * computes the probability distribution across the bands,
 * and then calculates the entropy.
 * The result is normalized by dividing by ln(NUM_BANDS).
 *
 * @param spl_db Array of 26 third-octave band SPL values in decibels.
 * @param mean_db Array of the 26 mean dB SPL for a sample duration.
 *                If NULL, do not prewhiten the spectra.
 * @return Normalized spectral entropy (range 0 to 1).
 */
float compute_spectral_entropy(const float spl_db[NUM_BANDS],
                               const float mean_db[NUM_BANDS]) {
  float linear_power[NUM_BANDS];
  float sum_power = 0.0f;

  // Convert SPL in dB to linear power scale: power ∝ 10^(dB/10)
  for (int i = 0; i < NUM_BANDS; i++) {
    linear_power[i] = powf(10.0f, spl_db[i] / 10.0f);
    if (mean_db) {
      // Prewhiten using the given means
      float linear_mean = powf(10.0f, mean_db[i] / 10.0f);
      linear_power[i] /= linear_mean;
    }
    sum_power += linear_power[i];
  }

  // Protect against division by zero.
  if (sum_power <= 0.0f) {
    return 0.0f;
  }

  float entropy = 0.0f;

  // Compute the entropy: H = -sum(p * log(p))
  for (int i = 0; i < NUM_BANDS; i++) {
    float p = linear_power[i] / sum_power;
    // Only add if p > 0 to avoid log(0)
    if (p > 0.0f) {
      entropy -= p * logf(p);
    }
  }

  // Normalize the entropy so that H_norm is between 0 and 1.
  float normalized_entropy = entropy / logf(NUM_BANDS);

  return normalized_entropy;
}
