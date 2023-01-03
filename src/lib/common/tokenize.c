#include <string.h>
#include "FreeRTOS.h"
#include "tokenize.h"

// Number of token pointers to allocate by default
#define DEFAULT_NUM_TOKENS 16

/*!
  Function to tokenize string and return array and number of tokens

  \param[in] line string to tokenize(String will be modified by function)
  \param[in] len length of string to tokenize
  \param[in] token token character to use
  \param[out] tokenCount pointer that holds total number of tokens
  \return Pointer to token array (MUST be freed by caller!)

  Note: last token is not zero terminated.
*/
char **tokenize(char *line, size_t len, char token, size_t *tokenCount) {
  *tokenCount = 0;
  char **tokens;
  uint32_t maxTokens = DEFAULT_NUM_TOKENS;

  configASSERT(tokenCount != NULL);
  tokens = pvPortMalloc(maxTokens * sizeof(char *));
  configASSERT(tokens != NULL);

  // Start first token at beginning of line
  tokens[*tokenCount] = line;

  for(uint16_t idx = 0; idx < len; idx++) {
    if(line[idx] == token) {
      line[idx] = 0;

      // Deal with empty (previous) token
      if((&line[idx] - tokens[*tokenCount]) == 0) {
        tokens[*tokenCount] = NULL;
      }

      (*tokenCount)++;

      // If we didn't allocate enough space for all of the tokens
      // double the size of the token array and copy the data over
      if(*tokenCount == maxTokens){
        char **ptmpTokens = pvPortMalloc(maxTokens * 2 * sizeof(char *));
        configASSERT(ptmpTokens != NULL);

        memcpy(ptmpTokens, tokens, (maxTokens * sizeof(char *)));
        vPortFree(tokens);
        tokens = ptmpTokens;

        // Update the new maximum
        maxTokens *= 2;
      }

      // Start the next token
      tokens[*tokenCount] = &line[idx+1];
    }
  }

  // There were no tokens, no need to keep allocated memory
  if(*tokenCount == 0) {
    vPortFree(tokens);
    tokens = NULL;
  } else {
    // Deal with last token being empty
    if((&line[len] - tokens[*tokenCount]) == 0) {
      tokens[*tokenCount] = NULL;
    }
    // Arrays are zero indexed but we want to return actual count
    (*tokenCount)++;
  }

  return tokens;
}
