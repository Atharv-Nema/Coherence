add_library(var_validity_checker
    consume_checker.cpp
    override_checker.cpp
    var_validity_checker.cpp
)

target_link_libraries(var_validity_checker PUBLIC
    ast 
    global_utils
    general_utils
)

target_include_directories(var_validity_checker PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})