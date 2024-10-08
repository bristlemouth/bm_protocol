// #pragma once
// #include <stdint.h>

// #ifdef __cplusplus
// extern "C" {
// #endif // __cplusplus

// typedef struct {
//     uint8_t stateEnum; // Should match to an ENUM corresponding to state.
//     const char * stateName; // MUST NOT BE NULL
//     void (*run)(void); // MUST NOT BE NULL
//     void (*onStateExit)(void); // Null or function pointer
//     void (*onStateEntry)(void); // Null or function pointer
// } libSmState_t;

// typedef const libSmState_t* (*checkTransitionsForNextState_t)(uint8_t);

// typedef struct {
//     const libSmState_t * current_state;
//     checkTransitionsForNextState_t checkTransitionsForNextState;
// } libSmContext_t;

// void libSmInit(libSmContext_t& ctx, const libSmState_t& init_state, checkTransitionsForNextState_t checkTransitionsForNextState);
// void libSmRun(libSmContext_t& ctx);
// const char * libSmGetCurrentStateName(const libSmContext_t& ctx);
// uint8_t getCurrentStateEnum(const libSmContext_t& ctx);
// #ifdef __cplusplus
// }
// #endif // __cplusplus
//
// lets get another git sha for testing dfu updates!