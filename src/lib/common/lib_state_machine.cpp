// #include "lib_state_machine.h"
// #include "FreeRTOS.h"

// /*!
//   Initialize the state machine

//   \param[in] ctx Pointer to the state machine context.
//   \param[in] init_state Pointer to the state to start the state machine at.
//   \param[in] checkTransitionsForNextState Pointer to the function which will check for state transitions.
//   \return N/A
// */
// void libSmInit(libSmContext_t& ctx, const libSmState_t& init_state, checkTransitionsForNextState_t checkTransitionsForNextState){
//     configASSERT(checkTransitionsForNextState != NULL);
//     ctx.current_state = &init_state;
//     ctx.checkTransitionsForNextState = checkTransitionsForNextState;
// }

// /*!
//   Run an iteration of the state machine

//   \param[in] ctx Pointer to the state machine context.
//   \return N/A
// */
// void libSmRun(libSmContext_t& ctx) {
//     configASSERT(ctx.current_state != NULL);
//     configASSERT(ctx.current_state->run);
//     ctx.current_state->run();
//     const libSmState_t * next_state = ctx.checkTransitionsForNextState(ctx.current_state->stateEnum);
//     configASSERT(next_state != NULL);
//     if (ctx.current_state != next_state) {
//         if(ctx.current_state->onStateExit){
//             ctx.current_state->onStateExit();
//         }
//         ctx.current_state = next_state;
//         if(ctx.current_state->onStateEntry){
//             ctx.current_state->onStateEntry();
//         }
//     }
// }

// /*!
//   Returns the current state's string name.

//   \param[in] ctx Pointer to the state machine context.
//   \return The name of the current state.
// */
// const char * libSmGetCurrentStateName(const libSmContext_t& ctx) {
//     configASSERT(ctx.current_state->stateName != NULL);
//     return ctx.current_state->stateName;
// }

// /*!
//   Returns the current state's string name.

//   \param[in] ctx Pointer to the state machine context.
//   \return The name of the current state.
// */
// uint8_t getCurrentStateEnum(const libSmContext_t& ctx) {
//   return ctx.current_state->stateEnum;
// }
