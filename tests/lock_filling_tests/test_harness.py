import os
import pathlib
import subprocess
import pytest

ROOT = pathlib.Path(__file__).resolve().parent
LOCK_TEST_DATA = ROOT / "lock_test_data"

RUNNER = os.environ.get("LOCK_TEST_RUNNER")
if RUNNER is None:
    raise RuntimeError(
        "LOCK_TEST_RUNNER not set. "
    )

def case_dirs():
    for p in sorted(LOCK_TEST_DATA.iterdir()):
        if p.is_dir():
            yield p

@pytest.mark.parametrize("case_dir", case_dirs(), ids=lambda p: p.name)
def test_lock_filling(case_dir):
    proc = subprocess.run(
        [RUNNER, str(case_dir)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    if proc.returncode != 0:
        pytest.fail(
            f"\ncase: {case_dir}\n"
            f"\nstderr:\n{proc.stderr}"
        )
