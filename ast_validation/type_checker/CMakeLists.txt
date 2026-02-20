add_library(type_checker
    core_type_checker.cpp
    initialization_checker.cpp
    return_checker.cpp
    full_type_checker.cpp
    type_checker_utils.cpp
)

target_link_libraries(type_checker PUBLIC 
    ast
    global_utils    
    general_utils
)

target_include_directories(type_checker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})