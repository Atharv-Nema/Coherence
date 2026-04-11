import argparse
import subprocess
import time
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import shutil
import sys
from contextlib import contextmanager
from dataclasses import dataclass


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
    exe_path = Path(cmd[0])
    if not exe_path.exists():
        print(f"Executable not found: {exe_path}")
        sys.exit(1)
    start = time.perf_counter()
    for _ in range(5):
        r = run(cmd)
    elapsed = time.perf_counter() - start
    if r.returncode != 0:
        print(f"Execution failed: {' '.join(cmd)}")
        print(f"stdout: {r.stdout}")
        print(f"stderr: {r.stderr}")
        sys.exit(1)
    return elapsed / 5


def time_compilation(cmd):
    """Time compilation and return elapsed time."""
    start = time.perf_counter()
    for _ in range(5):
        run(cmd, check=True)
    elapsed = time.perf_counter() - start
    return elapsed / 5


@contextmanager
def temporary_directories(*dirs: Path):
    """Context manager that creates directories and cleans them up on exit."""
    created = []
    try:
        for d in dirs:
            d.mkdir(parents=True, exist_ok=True)
            created.append(d)
        yield created
    finally:
        for d in created:
            if d.exists():
                shutil.rmtree(d)


@dataclass
class BenchmarkConfig:
    compiler: str
    ponyc: str
    output_dir: Path
    root_dir: Path


def compile_coherence(compiler: str, input_file: Path, output_dir: Path, optimize: bool = False):
    """Compile a Coherence file and check for errors."""
    cmd = [
        compiler,
        "--input-file", str(input_file),
        "--output-dir", str(output_dir),
        "--optimize", "true" if optimize else "false"
    ]
    print(f"  Compiling: {' '.join(cmd)}")
    result = run(cmd, check=True)
    
    expected_out = output_dir / "out"
    if not expected_out.exists():
        print(f"Compilation did not produce expected output: {expected_out}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result

def compile_pony(ponyc: str, source_dir: Path, output_dir: Path, binary_name: str = "main", release: bool = False):
    """Compile a Pony project and check for errors."""
    cmd = [ponyc]
    if release:
        cmd.extend(["-D", "release"])
    cmd.extend(["-o", str(output_dir), "-b", binary_name, str(source_dir)])
    
    print(f"    Compiling: {' '.join(cmd)}")
    result = run(cmd, check=True)
    
    expected_out = output_dir / binary_name
    if not expected_out.exists():
        print(f"Compilation did not produce expected output: {expected_out}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result

def benchmark_struct_access(config: BenchmarkConfig):
    """Benchmark struct access"""
    print("Benchmarking Struct Access")
    
    sa_dir = config.root_dir / "benchmarks" / "struct_access"
    sa_coh = sa_dir / "coherence_implementation" / "prog.coh"
    sa_pony = sa_dir / "pony_implementation"

    # Verify source files exist
    if not sa_coh.exists():
        print(f"Coherence source not found: {sa_coh}")
        sys.exit(1)
    if not sa_pony.exists():
        print(f"Pony source directory not found: {sa_pony}")
        sys.exit(1)

    # Define output directories
    bin_dirs = {
        "coh_o0": sa_dir / "bin_coh_o0",
        "coh_o3": sa_dir / "bin_coh_o3",
        "pony_o0": sa_dir / "bin_pony_o0",
        "pony_o3": sa_dir / "bin_pony_o3"
    }

    with temporary_directories(*bin_dirs.values()):
        compile_coherence(config.compiler, sa_coh, bin_dirs["coh_o0"], optimize=False)
        compile_coherence(config.compiler, sa_coh, bin_dirs["coh_o3"], optimize=True)
        compile_pony(config.ponyc, sa_pony, bin_dirs["pony_o0"], release=False)
        compile_pony(config.ponyc, sa_pony, bin_dirs["pony_o3"], release=True)

        t_coh_o0 = time_exe([str(bin_dirs["coh_o0"] / "out")])
        t_pony_o0 = time_exe([str(bin_dirs["pony_o0"] / "main"), "--ponymaxthreads", "16"])
        t_coh_o3 = time_exe([str(bin_dirs["coh_o3"] / "out")])
        t_pony_o3 = time_exe([str(bin_dirs["pony_o3"] / "main"), "--ponymaxthreads", "16"])

        print(f"  Results: coh_o0={t_coh_o0:.3f}s, pony_o0={t_pony_o0:.3f}s, coh_o3={t_coh_o3:.3f}s, pony_o3={t_pony_o3:.3f}s")

        # Plot
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
        plt.savefig(config.output_dir / "struct_access_report.png")
        plt.close()

def benchmark_ring_token(config: BenchmarkConfig):
    print("Benchmarking Ring Token")
    
    rt_dir = config.root_dir / "benchmarks" / "ring_token"
    rt_coh = rt_dir / "coherence_implementation" / "prog.coh"
    rt_pony = rt_dir / "pony_implementation"

    # Verify source files exist
    if not rt_coh.exists():
        print(f"Coherence source not found: {rt_coh}")
        sys.exit(1)
    if not rt_pony.exists():
        print(f"Pony source directory not found: {rt_pony}")
        sys.exit(1)

    bin_dirs = {
        "coh": rt_dir / "bin_coh_o0",
        "pony": rt_dir / "bin_pony_o0"
    }

    with temporary_directories(*bin_dirs.values()):
        # Compile
        compile_coherence(config.compiler, rt_coh, bin_dirs["coh"], optimize=False)
        compile_pony(config.ponyc, rt_pony, bin_dirs["pony"], release=False)

        # Time executions
        t_rt_coh = time_exe([str(bin_dirs["coh"] / "out")])
        t_rt_pony = time_exe([str(bin_dirs["pony"] / "main"), "--ponymaxthreads", "16"])

        print(f"  Results: coh={t_rt_coh:.3f}s, pony={t_rt_pony:.3f}s")

        # Plot
        plt.figure(figsize=(8, 6))
        plt.bar(['Coherence', 'Pony'], [t_rt_coh, t_rt_pony], color=['#0000FF', '#FF0000'])
        plt.ylabel('Time (s)')
        plt.title('Ring Token')
        plt.grid(axis='y', linestyle='--', alpha=0.5)
        plt.savefig(config.output_dir / "ring_token_report.png")
        plt.close()

    print("  Done.")


def get_func_str(n: int):
    return f"""
func f{n}() => unit {{
    atomic {{
        {f"f{n - 1}();" if n != 0 else ""}
        *(new locked<L{n}>[1] unit(()));
    }}
    return ();
}}
    """

def generate_phi(n: int, path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        for i in range(n):
            f.write(get_func_str(i))
        main_actor = """
actor Main {
    new create() {}
}
"""
        f.write(main_actor)

def benchmark_compilation_time(config: BenchmarkConfig, sizes: list[int] = None):
    print("Benchmarking compilation time")
    
    if sizes is None:
        sizes = [500, 1000, 2000, 3000, 4000, 5000, 6000]
    
    compile_dir = config.root_dir / "benchmarks" / "compilation_time"
    
    with temporary_directories(compile_dir):
        times = []
        
        for n in sizes:
            print(f"  Testing n={n}...")
            
            # Generate the code
            source_file = compile_dir / f"phi_{n}.coh"
            generate_phi(n, source_file)
            
            # Time compilation
            cmd = [
                config.compiler,
                "--input-file", str(source_file),
                "--only-typecheck", "true",
            ]
            
            elapsed = time_compilation(cmd)
            times.append(elapsed)
            print(f"    n={n}: {elapsed:.3f}s")
        
        # Fit quadratic curve for comparison
        coeffs = np.polyfit(sizes, times, 2)
        fitted = np.polyval(coeffs, sizes)
        
        # Print results
        print("\n  Results:")
        print(f"    {'n':>6} | {'Time (s)':>10}")
        print(f"    {'-'*6}-+-{'-'*10}")
        for n, t in zip(sizes, times):
            print(f"    {n:>6} | {t:>10.3f}")
        
        print(f"\n  Quadratic fit: {coeffs[0]:.2e}n² + {coeffs[1]:.2e}n + {coeffs[2]:.2e}")
        
        # Plot
        plt.figure(figsize=(10, 6))
        
        # Actual times
        plt.scatter(sizes, times, color='#3b82f6', s=100, zorder=5, label='Measured')
        plt.plot(sizes, times, color='#3b82f6', linewidth=2, alpha=0.7)
        
        # Quadratic fit
        x_smooth = np.linspace(min(sizes), max(sizes), 100)
        y_smooth = np.polyval(coeffs, x_smooth)
        plt.plot(x_smooth, y_smooth, '--', color='#ef4444', linewidth=2, 
                 label=f'Quadratic fit: {coeffs[0]:.2e}n²')
        
        plt.xlabel('Code Size (n)')
        plt.ylabel('Compilation Time (s)')
        plt.title('Compilation Time vs Code Size')
        plt.legend()
        plt.grid(True, linestyle='--', alpha=0.3)
        
        plt.savefig(config.output_dir / "compilation_time_report.png")
        plt.close()
        
        # Also save a log-log plot to verify polynomial degree
        plt.figure(figsize=(10, 6))
        plt.loglog(sizes, times, 'o-', color='#3b82f6', linewidth=2, markersize=8, label='Measured')
        
        # Reference lines for O(n) and O(n²)
        ref_n = np.array(sizes, dtype=float)
        scale_linear = times[-1] / sizes[-1]
        scale_quad = times[-1] / (sizes[-1] ** 2)
        plt.loglog(sizes, scale_linear * ref_n, '--', color='#22c55e', alpha=0.7, label='O(n)')
        plt.loglog(sizes, scale_quad * ref_n ** 2, '--', color='#ef4444', alpha=0.7, label='O(n²)')
        
        plt.xlabel('Code Size (n)')
        plt.ylabel('Compilation Time (s)')
        plt.title('Compilation Time vs Code Size (Log-Log Scale)')
        plt.legend()
        plt.grid(True, which="both", linestyle='--', alpha=0.3)
        plt.savefig(config.output_dir / "compilation_time_loglog_report.png")
        plt.close()

    print("  Done.")


def main():
    parser = argparse.ArgumentParser(description="Run Coherence benchmarks")
    parser.add_argument("--compiler", required=True, help="Path to Coherence compiler")
    parser.add_argument("--ponyc", required=True, help="Path to ponyc")
    parser.add_argument("--output-dir", required=True, help="Directory for benchmark reports")
    parser.add_argument("--root-dir", required=True, help="Project root directory")
    args = parser.parse_args()

    config = BenchmarkConfig(
        compiler=args.compiler,
        ponyc=args.ponyc,
        output_dir=Path(args.output_dir),
        root_dir=Path(args.root_dir)
    )
    
    config.output_dir.mkdir(parents=True, exist_ok=True)

    if not config.root_dir.exists():
        print(f"Root directory not found: {config.root_dir}")
        sys.exit(1)
    
    # benchmark_struct_access(config)
    benchmark_ring_token(config)
    # benchmark_compilation_time(config)
    sys.exit(0)


if __name__ == "__main__":
    main()