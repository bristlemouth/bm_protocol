#pragma once

#define NUM_BANDS 26

#ifdef __cplusplus
extern "C" {
#endif

float compute_spectral_entropy(const float spl_db[NUM_BANDS], const float mean_db[NUM_BANDS]);

#ifdef __cplusplus
}
#endif
