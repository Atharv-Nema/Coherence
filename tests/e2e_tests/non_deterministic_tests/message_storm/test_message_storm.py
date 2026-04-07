import pathlib
from e2e_tests.test_utilities import *

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def test_message_storm(tmp_path):
    prog_path = TESTS_ROOT / "prog.coh"
    output = compile_and_run(prog_path, tmp_path)
    assert output.count(1) == 10000, "expected number of 1's to be 10000"
    assert output.count(-1) == 10000, "expected number of -1's to be 10000"
