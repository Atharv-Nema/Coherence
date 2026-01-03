import os
import pathlib
import pytest
import json
from e2e_tests.test_utilities import *

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def find_test_dirs():
    out = []
    for d in sorted(TESTS_ROOT.iterdir()):
        if d.is_dir() and (d / "test_info.json").is_file() \
           and (d / "prog.coh").is_file():
            out.append(d)
    return out

TEST_DIRS = find_test_dirs()

@pytest.mark.parametrize("test_dir", TEST_DIRS, ids=[d.name for d in TEST_DIRS])
def test_deterministic(test_dir, tmp_path):
    coh = test_dir / "prog.coh"
    with open(test_dir / 'test_info.json', 'r') as file:
        data = json.load(file)

    # The [COH_COMPILER] environment variable is set by cmake
    compiler = os.environ.get("COH_COMPILER")
    assert compiler, "COH_COMPILER env var not set to coherencec path"

    r = run([compiler, "--input-file", str(coh), "--output-dir", str(tmp_path)])

    if data["compiles"]:
        assert r.returncode == 0, "Expected to compile, but did not"
    else:
        assert r.returncode != 0, "Expected not to compile, but did"
        return

    exe = tmp_path / "out"
    assert exe.exists(), f"expected executable not found: {exe}"

    rr = run([str(exe)])
    prog_out = to_list(rr.stdout)
    assert data["output"] == prog_out, "Outputs do not match"