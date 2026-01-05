add_library(global_utils alpha_renaming.cpp)
target_include_directories(global_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(global_utils PUBLIC ast)