set(BENCHMARK_RESULTS_DIR "${CMAKE_BINARY_DIR}/benchmarks/benchmark_results")

# Ensure the results directory exists
find_program(PONYC_PATH ponyc REQUIRED)


add_test(
  NAME benchmarks
  COMMAND "${COH_VENV_PY}" "${CMAKE_SOURCE_DIR}/benchmarks/run_benchmarks.py"
          --compiler "$<TARGET_FILE:${COH_COMPILER_TARGET}>"
          --ponyc "${PONYC_PATH}"
          --output-dir "${BENCHMARK_RESULTS_DIR}"
          --root-dir "${CMAKE_SOURCE_DIR}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)