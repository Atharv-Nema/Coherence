import os
import pathlib
import subprocess
import difflib
import pytest

TESTS_ROOT = pathlib.Path(__file__).resolve().parents[0]

def norm(s: str) -> str:
    return s.replace("\r\n", "\n").replace("\r", "\n")

def read(p: pathlib.Path) -> str:
    return p.read_text(encoding="utf-8")

def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)

def find_test_dirs():
    out = []
    for d in sorted(TESTS_ROOT.iterdir()):
        if d.is_dir() and (d / "compiler_out.txt").is_file() \
           and (d / "prog_out.txt").is_file()\
           and (d / "prog.coh").is_file():
            out.append(d)
    return out

TEST_DIRS = find_test_dirs()

@pytest.mark.parametrize("test_dir", TEST_DIRS, ids=[d.name for d in TEST_DIRS])
def test_deterministic(test_dir, tmp_path):
    coh = test_dir / "prog.coh"
    exp_comp_out = norm(read(test_dir / "compiler_out.txt"))
    exp_prog_out = norm(read(test_dir / "prog_out.txt"))

    # The [COH_COMPILER] environment variable is set by cmake
    compiler = os.environ.get("COH_COMPILER")
    assert compiler, "COH_COMPILER env var not set to coherencec path"

    r = run([compiler, "--input-file", str(coh), "--output-dir", str(tmp_path)])
    got_comp_out = norm(r.stdout + r.stderr)

    if got_comp_out != exp_comp_out:
        diff = "".join(difflib.unified_diff(
            exp_comp_out.splitlines(True),
            got_comp_out.splitlines(True),
            fromfile="expected compiler_out.txt",
            tofile="got compiler output",
        ))
        pytest.fail("compiler output mismatch:\n" + diff)

    if r.returncode != 0:
        # Compiler is expected to fail
        return

    # 4) run produced program
    exe = tmp_path / "out"
    assert exe.exists(), f"expected executable not found: {exe}"

    rr = run([str(exe)])
    got_prog = norm(rr.stdout + rr.stderr)

    if got_prog != exp_prog_out:
        diff = "".join(difflib.unified_diff(
            exp_prog_out.splitlines(True),
            got_prog.splitlines(True),
            fromfile="expected prog_out.txt",
            tofile="got program output",
        ))
        pytest.fail("program output mismatch:\n" + diff)