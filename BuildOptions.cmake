###################
# Project Options #
###################

include(CMakeDependentOption)
include(CheckIPOSupported)

option(BUILD_WITH_STATIC_ANALYSIS
  "Enable static analysis output when building the project."
  OFF)
option(DISABLE_STACK_PROTECTION
  "Disable stack smashing protection (-fno-stack-protector)."
  OFF)
set(USE_SANITIZER
    "" CACHE STRING
    "Compile with a sanitizer. Options are: Address, Memory, Leak, Undefined, Thread, 'Address;Undefined'"
)

if("${ENABLE_LTO}")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(USE_SANITIZER MATCHES "([Aa]ddress)")
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
elseif(USE_SANITIZER MATCHES "([Tt]hread)")
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
elseif(USE_SANITIZER MATCHES "([Uu]ndefined)")
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
elseif(USE_SANITIZER MATCHES "([Ll]eak)")
  add_compile_options(-fsanitize=leak)
  add_link_options(-fsanitize=leak)
elseif(USE_SANITIZER MATCHES "([Mm]emory)")
  add_compile_options(-fsanitize=memory)
  add_link_options(-fsanitize=memory)
elseif(USE_SANITIZER MATCHES "([Aa]ddress);([Uu]ndefined)")
  add_compile_options(-fsanitize=address,undefined)
  add_link_options(-fsanitize=address,undefined)
elseif(NOT "${USE_SANITIZER}" STREQUAL "")
  message(FATAL_ERROR "Unsupported value of USE_SANITIZER: ${USE_SANITIZER}")
endif()

if(DISABLE_BUILTINS)
  add_compile_options(-fno-builtin)
endif()

if(DISABLE_STACK_PROTECTION)
  add_compile_options(-fno-stack-protector)
endif()

# Export compile_commands.json file.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
