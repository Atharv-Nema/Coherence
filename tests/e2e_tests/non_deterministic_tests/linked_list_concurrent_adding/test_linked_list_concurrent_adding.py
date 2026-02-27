import pathlib
from e2e_tests.test_utilities import *

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def test_linked_list_concurrent_adding(tmp_path):
    prog_path = TESTS_ROOT / "prog.coh"
    output = compile_and_run(prog_path, tmp_path)
    output.sort()
    for i in range(10000):
        assert output[i] == i, "output is not a permutation of 0, 1 ... 9999"
