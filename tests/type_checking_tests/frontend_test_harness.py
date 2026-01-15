import os
import pathlib
import pytest
from e2e_tests.test_utilities import *

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def get_coh_files(dir: pathlib.Path) -> list[pathlib.Path]:
    out = []
    for file in sorted(dir.iterdir()):
        if file.is_file() and file.suffix == "coh":
            out.append(file)
    return out

WELL_TYPED_PROGS = get_coh_files(TESTS_ROOT / "well_typed_programs")
@pytest.mark.parametrize("prog_path", WELL_TYPED_PROGS, ids = [d.name for d in WELL_TYPED_PROGS])
def test_well_typed(prog_path: pathlib.Path):
    compiler = os.environ.get("COH_COMPILER")
    r = run([compiler, "--input-file", str(prog_path), "--only-typecheck"])
    assert r.returncode == 0, f"Expected program {str(prog_path)} to type check"

ILL_TYPED_PROGS = get_coh_files(TESTS_ROOT / "ill_typed_programs")
@pytest.mark.parametrize("prog_path", ILL_TYPED_PROGS, ids = [d.name for d in ILL_TYPED_PROGS])
def test_ill_typed(prog_path: pathlib.Path):
    compiler = os.environ.get("COH_COMPILER")
    r = run([compiler, "--input-file", str(prog_path), "--only-typecheck"])
    assert r.returncode != 0, f"Expected program {str(prog_path)} to not type check"