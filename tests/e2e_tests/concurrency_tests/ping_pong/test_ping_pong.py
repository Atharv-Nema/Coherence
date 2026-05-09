import pathlib
from e2e_tests.test_utilities import *

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def test_ping_pong(tmp_path):
    prog_path = TESTS_ROOT / "prog.coh"
    output = compile_and_run(prog_path, tmp_path)
    assert len(output) == 1000 * 1000, f"length of output must be 1000 * 1000, got {len(output)}"
    int_counts: dict[int, int] = {}
    for i in output:
        if i not in int_counts:
            int_counts[i] = 0
        int_counts[i] += 1
    for i in range(1000):
        assert int_counts[i + 1] == 1000, f"expected numbers of {i + 1}'s is 1000, got {int_counts[i + 1]}"
