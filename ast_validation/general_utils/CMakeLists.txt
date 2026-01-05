add_library(general_utils
    utils.cpp
)

target_link_libraries(general_utils PUBLIC 
    ast
    global_utils
)

target_include_directories(general_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})