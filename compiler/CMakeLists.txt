add_executable(coherence
    main.cpp
)

target_link_libraries(coherence PUBLIC
    ast
    lexer_parser
    type_checker
    codegen
)

# Include headers from these modules
# target_include_directories(coherence PRIVATE
#     ${CMAKE_SOURCE_DIR}/ast
#     ${CMAKE_SOURCE_DIR}/type_checker
#     ${CMAKE_SOURCE_DIR}/lexer_parser
# )
target_compile_definitions(coherence PRIVATE
    COH_RUNTIME_LIB_PATH="$<TARGET_FILE:runtime>"
)

message(STATUS "Coherence compiler executable configured successfully.")
