find_package(nlohmann_json REQUIRED)
add_executable(lock_test_runner
    test_runner.cpp
    callable_lock_info.cpp
)

target_include_directories(lock_test_runner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lock_test_runner PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(lock_test_runner PUBLIC
    ast 
    ast_validation
    lexer_parser
)