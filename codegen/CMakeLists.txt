add_library(codegen
    "alpha_renaming.cpp"
    "codegen_utils.cpp"
    "codegen.cpp"
)

target_include_directories(codegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(codegen PUBLIC 
    ast
    global_utils
    runtime
    type_checker
)