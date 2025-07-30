#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void insert_spectrum_into_list(const unsigned int num_bands, float *const spl_db);
void clear_spectral_entropy_list(void);
float calc_min_spectral_entropy(const unsigned int num_bands,
                                               float const *const means);

#ifdef __cplusplus
}
#endif
