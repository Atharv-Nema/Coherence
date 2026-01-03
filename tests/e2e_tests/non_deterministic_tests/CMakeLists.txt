add_test(
  NAME e2e_non_deterministic
  COMMAND "${COH_VENV_PY}" -B -m pytest -q "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_tests_properties(e2e_non_deterministic PROPERTIES
  ENVIRONMENT "COH_COMPILER=$<TARGET_FILE:${COH_COMPILER_TARGET}>;PYTHONPATH=${CMAKE_SOURCE_DIR}/tests"
  DEPENDS coh_venv
)
