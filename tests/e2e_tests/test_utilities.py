import subprocess
import os

def to_list(s: str) -> list[int]:
    return [int(line) for line in s.splitlines() if line.strip()]

def run(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)

def compile_and_run(prog_path, tmp_path) -> list[int]:
    compiler = os.environ.get("COH_COMPILER")
    assert compiler, "COH_COMPILER env var not set to coherencec path"
    r = run([compiler, "--input-file", str(prog_path), "--output-dir", str(tmp_path)])
    assert r.returncode == 0, "Compilation failed"
    exe = tmp_path / "out"
    assert exe.exists(), f"expected executable not found: {exe}"
    rr = run([str(exe)])
    return to_list(rr.stdout)