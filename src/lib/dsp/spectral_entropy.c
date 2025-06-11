#include "spectral_entropy.h"
#include "bm_os.h"
#include "ll.h"
#include <math.h>

typedef struct _SpectralEntropyArgs {
  unsigned int num_bands;
  float const *const means;
} SpectralEntropyArgs;

static LL spectra = {0};
static float min_entropy = 1.0;

/**
 * @brief Computes the normalized spectral entropy.
 *
 * Converts an array of SPL values (in dB) to a linear scale,
 * computes the probability distribution across the bands,
 * and then calculates the entropy.
 * The result is normalized by dividing by ln(num_bands).
 *
 * @param spl_db Array of third-octave band SPL values in decibels.
 * @param mean_db Array of mean dB SPL for a sample duration.
 *                If NULL, do not prewhiten the spectra.
 * @return Normalized spectral entropy (range 0 to 1).
 *         If there's an error return NAN.
 */
static float compute_spectral_entropy(const unsigned int num_bands, float const *const spl_db,
                                      float const *const mean_db) {
  float *linear_power = bm_malloc(num_bands * sizeof(float));
  if (linear_power == NULL) {
    return NAN;
  }
  float sum_power = 0.0f;

  // Convert SPL in dB to linear power scale: power ∝ 10^(dB/10)
  for (unsigned int i = 0; i < num_bands; i++) {
    linear_power[i] = powf(10.0f, spl_db[i] / 10.0f);
    if (mean_db) {
      // Prewhiten using the given means
      float linear_mean = powf(10.0f, mean_db[i] / 10.0f);
      linear_power[i] /= linear_mean;
    }
    sum_power += linear_power[i];
  }

  // Protect against division by zero
  if (sum_power <= 0.0f) {
    bm_free(linear_power);
    return NAN;
  }

  // Compute the entropy: H = -sum(p * log(p))
  float entropy = 0.0f;
  for (unsigned int i = 0; i < num_bands; i++) {
    float p = linear_power[i] / sum_power;
    // Only add if p > 0 to avoid log(0)
    if (p > 0.0f) {
      entropy -= p * logf(p);
    }
  }
  bm_free(linear_power);

  // Normalize the entropy between 0 and 1
  return entropy / logf(num_bands);
}

void insert_spectrum_into_list(const unsigned int num_bands, float *const spl_db) {
  size_t data_size = num_bands * sizeof(float);
  LLItem *item = NULL;
  static uint32_t id = 0;
  item = ll_create_item(item, spl_db, data_size, id++);
  ll_item_add(&spectra, item);
}

static BmErr compute_entropies_and_find_minimum(void *data, void *arg) {
  float *const band_levels = (float *const)data;
  SpectralEntropyArgs *args = (SpectralEntropyArgs *)arg;
  const float entropy = compute_spectral_entropy(args->num_bands, band_levels, args->means);
  if (entropy < min_entropy) {
    min_entropy = entropy;
  }
  return BmOK;
}

void clear_spectral_entropy_list(void) {
  while (spectra.head != NULL) {
    ll_remove(&spectra, spectra.head->id);
  }
}

float calc_min_spectral_entropy_and_clear_list(const unsigned int num_bands,
                                               float const *const means) {
  min_entropy = 1.0f;
  SpectralEntropyArgs arg = {.num_bands = num_bands, .means = means};
  ll_traverse(&spectra, compute_entropies_and_find_minimum, &arg);
  clear_spectral_entropy_list();
  return min_entropy;
}
