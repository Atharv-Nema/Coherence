add_library(codegen
    "generator_state.hpp"
)

target_include_directories(codegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(codegen PUBLIC 
    ast
    global_utils
    type_checker
)