add_subdirectory(lock_test_runner)

add_test(
  NAME lock_filling_tests
  COMMAND "${COH_VENV_PY}" -B -m pytest -q
          "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_tests_properties(lock_filling_tests PROPERTIES
  ENVIRONMENT
    "LOCK_TEST_RUNNER=$<TARGET_FILE:lock_test_runner>;
     PYTHONPATH=${CMAKE_SOURCE_DIR}/tests"
)

set_tests_properties(lock_filling_tests PROPERTIES
  ENVIRONMENT "LOCK_TEST_RUNNER=$<TARGET_FILE:lock_test_runner>"
  DEPENDS lock_test_runner
)
