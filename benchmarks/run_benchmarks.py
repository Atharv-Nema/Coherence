import argparse
import subprocess
import time
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import shutil
import sys

def run(cmd, cwd=None, check=False):
    """Run a command and optionally check for errors."""
    result = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if check and result.returncode != 0:
        print(f"Command failed: {' '.join(cmd)}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result

def time_exe(cmd):
    """Time the execution of a command."""
    # First verify the executable exists
    exe_path = Path(cmd[0])
    if not exe_path.exists():
        print(f"Executable not found: {exe_path}")
        sys.exit(1)
    start = time.perf_counter()
    r = run(cmd)
    elapsed = time.perf_counter() - start
    if r.returncode != 0:
        print(f"Execution failed: {' '.join(cmd)}")
        print(f"stdout: {r.stdout}")
        print(f"stderr: {r.stderr}")
        sys.exit(1)
    return elapsed

def compile_coherence(compiler, input_file, output_dir, optimize=False):
    """Compile a Coherence file and check for errors."""
    cmd = [
        compiler,
        "--input-file", str(input_file),
        "--output-dir", str(output_dir),
        "--optimize", "true" if optimize else "false"
    ]
    print(f"  Compiling: {' '.join(cmd)}")
    result = run(cmd, check=True)
    
    # Verify output was created
    expected_out = output_dir / "out"
    if not expected_out.exists():
        print(f"Compilation did not produce expected output: {expected_out}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result

def compile_pony(ponyc, source_dir, output_dir, binary_name="main", release=False):
    """Compile a Pony project and check for errors."""
    cmd = [ponyc]
    if release:
        cmd.extend(["-D", "release"])  # This enables optimizations
    cmd.extend(["-o", str(output_dir), "-b", binary_name, str(source_dir)])
    
    print(f"  Compiling: {' '.join(cmd)}")
    result = run(cmd, check=True)
    
    # Verify output was created
    expected_out = output_dir / binary_name
    if not expected_out.exists():
        print(f"Compilation did not produce expected output: {expected_out}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--ponyc", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--root-dir", required=True)
    args = parser.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    root = Path(args.root_dir) / "benchmarks"

    # Verify root directory exists
    if not root.exists():
        print(f"Benchmark root directory not found: {root}")
        sys.exit(1)

    temp_dirs = []

    # Struct access benchmark
    print("Benchmarking Struct Access...")
    sa_dir = root / "struct_access"
    sa_coh = sa_dir / "coherence_implementation" / "prog.coh"
    sa_pony = sa_dir / "pony_implementation"

    # Verify source files exist
    if not sa_coh.exists():
        print(f"Coherence source not found: {sa_coh}")
        sys.exit(1)
    if not sa_pony.exists():
        print(f"Pony source directory not found: {sa_pony}")
        sys.exit(1)

    # Define bins with consistent naming
    sa_bins = {
        "coh_o0": sa_dir / "bin_coh_o0",
        "coh_o3": sa_dir / "bin_coh_o3",
        "pony_o0": sa_dir / "bin_pony_o0",
        "pony_o3": sa_dir / "bin_pony_o3"
    }
    
    for d in sa_bins.values():
        d.mkdir(parents=True, exist_ok=True)
        temp_dirs.append(d)

    # Compile Coherence
    compile_coherence(args.compiler, sa_coh, sa_bins["coh_o0"], optimize=False)
    compile_coherence(args.compiler, sa_coh, sa_bins["coh_o3"], optimize=True)
    
    # Compile Pony
    compile_pony(args.ponyc, sa_pony, sa_bins["pony_o0"], release=False)
    compile_pony(args.ponyc, sa_pony, sa_bins["pony_o3"], release=True)
    # Time executions
    print("  Timing executions...")
    t_coh_o0 = time_exe([str(sa_bins["coh_o0"] / "out")])
    t_pony_o0 = time_exe([str(sa_bins["pony_o0"] / "main"), "--ponymaxthreads", "16"])
    t_coh_o3 = time_exe([str(sa_bins["coh_o3"] / "out")])
    t_pony_o3 = time_exe([str(sa_bins["pony_o3"] / "main"), "--ponymaxthreads", "16"])

    print(f"  Results: coh_o0={t_coh_o0:.3f}s, pony_o0={t_pony_o0:.3f}s, coh_o3={t_coh_o3:.3f}s, pony_o3={t_pony_o3:.3f}s")

    # Plot Struct Access
    labels = ['Unoptimized', 'Optimized']
    x = np.arange(len(labels))
    width = 0.35
    plt.figure(figsize=(10, 6))
    plt.bar(x - width/2, [t_coh_o0, t_coh_o3], width, label='Coherence (Stack)', color='#3b82f6')
    plt.bar(x + width/2, [t_pony_o0, t_pony_o3], width, label='Pony (Heap)', color='#64748b')
    plt.ylabel('Time (s)')
    plt.title('Struct Access: Value vs Reference Semantics')
    plt.xticks(x, labels)
    plt.legend()
    plt.grid(axis='y', linestyle='--', alpha=0.3)
    plt.savefig(out_dir / "struct_access_report.png")
    plt.close()

    # Ring token benchmark
    print("Benchmarking Ring Token...")
    rt_dir = root / "ring_token"
    rt_coh = rt_dir / "coherence_implementation" / "prog.coh"
    rt_pony = rt_dir / "pony_implementation"

    # Verify source files exist
    if not rt_coh.exists():
        print(f"Coherence source not found: {rt_coh}")
        sys.exit(1)
    if not rt_pony.exists():
        print(f"Pony source directory not found: {rt_pony}")
        sys.exit(1)

    # Define bins
    rt_bins = {
        "coh": rt_dir / "bin_coh_o0",
        "pony": rt_dir / "bin_pony_o0"
    }
    
    for d in rt_bins.values():
        d.mkdir(parents=True, exist_ok=True)
        temp_dirs.append(d)

    # Compile
    compile_coherence(args.compiler, rt_coh, rt_bins["coh"], optimize=False)
    compile_pony(args.ponyc, rt_pony, rt_bins["pony"], release=False)

    # Time executions
    print("  Timing executions...")
    t_rt_coh = time_exe([str(rt_bins["coh"] / "out")])
    t_rt_pony = time_exe([str(rt_bins["pony"] / "main"), "--ponymaxthreads", "16"])

    print(f"  Results: coh={t_rt_coh:.3f}s, pony={t_rt_pony:.3f}s")

    # Plot Ring Token
    plt.figure(figsize=(8, 6))
    plt.bar(['Coherence', 'Pony'], [t_rt_coh, t_rt_pony], color=['#3b82f6', '#64748b'])
    plt.ylabel('Time (s)')
    plt.title('Ring Token: Messaging Overhead (Unoptimized)')
    plt.grid(axis='y', linestyle='--', alpha=0.5)
    plt.savefig(out_dir / "ring_token_report.png")
    plt.close()

    # Cleanup
    print("Cleaning up temporary directories...")
    for d in temp_dirs:
        if d.exists():
            shutil.rmtree(d)

    print(f"All reports generated in: {out_dir}")
    sys.exit(0)

if __name__ == "__main__":
    main()