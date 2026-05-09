import pathlib
from e2e_tests.test_utilities import *

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def test_ring_token(tmp_path):
    prog_path = TESTS_ROOT / "prog.coh"
    output = compile_and_run(prog_path, tmp_path)
    for i in range(10000):
        assert output[i] == 10000 - i, f"expected {i}th position to be {10000 - i}, got {output[i]}"
