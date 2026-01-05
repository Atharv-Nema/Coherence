add_subdirectory(general_utils)
add_subdirectory(type_checker)
add_subdirectory(var_validity_checker)

add_library(ast_validation
    ast_validator.cpp
)

target_include_directories(ast_validation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ast_validation PUBLIC 
    ast
    global_utils
    general_utils
    var_validity_checker
    type_checker
)

message(STATUS "ast validation configured successfully.")
