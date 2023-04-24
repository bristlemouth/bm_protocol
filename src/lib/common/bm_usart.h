#include <stdint.h>
#include "stm32u5xx.h"

static inline uint32_t usart_IsActiveFlag_ORE(USART_TypeDef *USARTx) {
  return ((READ_BIT(USARTx->ISR, USART_ISR_ORE) == (USART_ISR_ORE)) ? 1UL : 0UL);
}

static inline void usart_ClearFlag_ORE(USART_TypeDef *USARTx){
  WRITE_REG(USARTx->ICR, USART_ICR_ORECF);
}

static inline uint32_t usart_IsActiveFlag_RXNE(USART_TypeDef *USARTx) {
  return ((READ_BIT(USARTx->ISR, USART_ISR_RXNE_RXFNE) == (USART_ISR_RXNE_RXFNE)) ? 1UL : 0UL);
}

static inline uint32_t usart_IsEnabledIT_RXNE(USART_TypeDef *USARTx){
  return ((READ_BIT(USARTx->CR1, USART_CR1_RXNEIE_RXFNEIE) == (USART_CR1_RXNEIE_RXFNEIE)) ? 1UL : 0UL);
}

static inline void usart_DisableIT_RXNE(USART_TypeDef *USARTx){
  ATOMIC_CLEAR_BIT(USARTx->CR1, USART_CR1_RXNEIE_RXFNEIE);
}

static inline void usart_EnableIT_RXNE(USART_TypeDef *USARTx){
  ATOMIC_SET_BIT(USARTx->CR1, USART_CR1_RXNEIE_RXFNEIE);
}

static inline uint32_t usart_IsActiveFlag_TXE(USART_TypeDef *USARTx){
  return ((READ_BIT(USARTx->ISR, USART_ISR_TXE_TXFNF) == (USART_ISR_TXE_TXFNF)) ? 1UL : 0UL);
}

static inline uint32_t usart_IsEnabledIT_TXE(USART_TypeDef *USARTx){
  return ((READ_BIT(USARTx->CR1, USART_CR1_TXEIE_TXFNFIE) == (USART_CR1_TXEIE_TXFNFIE)) ? 1UL : 0UL);
}

static inline void usart_DisableIT_TXE(USART_TypeDef *USARTx){
  ATOMIC_CLEAR_BIT(USARTx->CR1, USART_CR1_TXEIE_TXFNFIE);
}

static inline void usart_EnableIT_TXE(USART_TypeDef *USARTx){
  ATOMIC_SET_BIT(USARTx->CR1, USART_CR1_TXEIE_TXFNFIE);
}

static inline uint8_t usart_ReceiveData8(USART_TypeDef *USARTx){
  return (uint8_t)(READ_BIT(USARTx->RDR, USART_RDR_RDR) & 0xFFU);
}

static inline void usart_TransmitData8(USART_TypeDef *USARTx, uint8_t Value){
  USARTx->TDR = Value;
}