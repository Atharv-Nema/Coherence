add_library(compute_lock_info
    callable_graph_builder.cpp
    compute_lock_info.cpp
    fill_lock_info.cpp
    sccs_finder.cpp
    stage_utils.cpp
)

target_link_libraries(compute_lock_info PUBLIC 
    ast
    global_utils
    general_utils   
)

target_include_directories(compute_lock_info PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})