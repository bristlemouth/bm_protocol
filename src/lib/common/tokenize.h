#pragma once

#ifdef __cplusplus
extern "C" {
#endif

char **tokenize(char *line, size_t len, char token, size_t *tokenCount);

#ifdef __cplusplus
}
#endif
