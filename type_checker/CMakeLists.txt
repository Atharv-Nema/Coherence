add_library(type_checker
    type_checker.cpp
    utils.cpp
)

target_include_directories(type_checker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(type_checker PUBLIC 
    ast
    global_utils
)

message(STATUS "TypeChecker configured successfully.")
