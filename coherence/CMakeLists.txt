find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(coherence
    main.cpp
)

target_link_libraries(coherence PUBLIC
    ast
    lexer_parser
    type_checker
    codegen
)

target_compile_definitions(coherence PRIVATE
    COH_RUNTIME_LIB_PATH="$<TARGET_FILE:runtime>"
)

target_link_libraries(coherence PUBLIC Boost::program_options)

message(STATUS "Coherence compiler executable configured successfully.")
