add_subdirectory(general_utils)
add_subdirectory(var_validity_checker)
add_subdirectory(declaration_collection)
add_subdirectory(type_checker)
add_subdirectory(compute_lock_info)

add_library(ast_validation
    ast_validator.cpp
)

target_include_directories(ast_validation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ast_validation PUBLIC 
    ast
    declaration_collection
    global_utils
    general_utils
    var_validity_checker
    type_checker
    compute_lock_info
)

message(STATUS "ast validation configured successfully.")
