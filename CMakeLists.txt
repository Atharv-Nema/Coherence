cmake_minimum_required(VERSION 3.16)
project(Coherence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(COH_VENV_DIR "${CMAKE_BINARY_DIR}/.venv")
set(COH_VENV_PY  "${COH_VENV_DIR}/bin/python")
set(COH_PY_REQS  "${CMAKE_SOURCE_DIR}/requirements.txt")

# Command to produce the python virtual environment
add_custom_command(
  OUTPUT "${COH_VENV_PY}"
  COMMAND ${Python3_EXECUTABLE} -m venv "${COH_VENV_DIR}"
  COMMAND "${COH_VENV_PY}" -m pip install -r "${COH_PY_REQS}"
  COMMENT "Creating venv + installing test requirements"
  VERBATIM
)

add_custom_target(coh_venv ALL DEPENDS "${COH_VENV_PY}")

add_subdirectory(ast)
add_subdirectory(global_utils)
add_subdirectory(lexer_parser)
add_subdirectory(type_checker)
add_subdirectory(runtime)
add_subdirectory(codegen)
add_subdirectory(coherence)

set(COH_COMPILER_TARGET coherence)

include(CTest)          # defines BUILD_TESTING option
if (BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()