find_package(Boost REQUIRED context)

add_library(runtime
    entry_point.cpp
    runtime_traps.cpp
)

target_include_directories(runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(runtime PUBLIC
    Boost::context
)

target_compile_features(runtime PUBLIC cxx_std_20)

message(STATUS "Runtime compiled successfully")
