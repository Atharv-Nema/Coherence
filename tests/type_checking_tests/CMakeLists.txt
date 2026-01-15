add_test(
  NAME type_checking_tests
  COMMAND "${COH_VENV_PY}" -B -m pytest -q "${CMAKE_CURRENT_SOURCE_DIR}/frontend_test_harness.py"
)

set_tests_properties(type_checking_tests PROPERTIES
  ENVIRONMENT "COH_COMPILER=$<TARGET_FILE:${COH_COMPILER_TARGET}>;PYTHONPATH=${CMAKE_SOURCE_DIR}/tests"
  DEPENDS coh_venv
)