add_library(declaration_collection
    declaration_collector.cpp
)

target_link_libraries(declaration_collection PUBLIC 
    ast
    global_utils
    general_utils
)

target_include_directories(declaration_collection PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})