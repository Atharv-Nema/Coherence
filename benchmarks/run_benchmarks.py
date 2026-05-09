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

def time_exe(cmd, runs=5):
    """Time the execution of a command, returning (mean, std) over multiple runs."""
    exe_path = Path(cmd[0])
    if not exe_path.exists():
        print(f"Executable not found: {exe_path}")
        sys.exit(1)
    times = []
    for _ in range(runs):
        start = time.perf_counter()
        r = run(cmd)
        times.append(time.perf_counter() - start)
    if r.returncode != 0:
        print(f"Execution failed: {' '.join(cmd)}")
        print(f"stdout: {r.stdout}")
        print(f"stderr: {r.stderr}")
        sys.exit(1)
    return float(np.mean(times)), float(np.std(times, ddof=1))


def time_compilation(cmd, runs=5):
    """Time compilation, returning (mean, std) over multiple runs."""
    times = []
    for _ in range(runs):
        start = time.perf_counter()
        run(cmd, check=True)
        times.append(time.perf_counter() - start)
    return float(np.mean(times)), float(np.std(times, ddof=1))


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


def compile_cpp(source_file: Path, output_file: Path):
    """Compile a C++ file with C++20 features."""
    cmd = [
        "g++",
        "-std=c++20",
        "-pthread",
        str(source_file),
        "-o", str(output_file)
    ]
    result = run(cmd, check=True)
    
    if not output_file.exists():
        print(f"Compilation did not produce expected output: {output_file}")
        print(f"stdout: {result.stdout}")
        print(f"stderr: {result.stderr}")
        sys.exit(1)
    return result


def benchmark_ping_pong(config: BenchmarkConfig):
    print("Benchmarking Ping Pong")

    pp_dir = config.root_dir / "benchmarks" / "ping_pong"
    pp_coh = pp_dir / "coherence_implementation" / "prog.coh"
    pp_pony = pp_dir / "pony_implementation"
    pp_cpp = pp_dir / "cpp_implementation" / "ping_pong.cpp"

    # Verify source files exist
    if not pp_coh.exists():
        print(f"Coherence source not found: {pp_coh}")
        sys.exit(1)
    if not pp_pony.exists():
        print(f"Pony source directory not found: {pp_pony}")
        sys.exit(1)
    if not pp_cpp.exists():
        print(f"C++ source not found: {pp_cpp}")
        sys.exit(1)

    bin_dirs = {
        "coh": pp_dir / "bin_coh_o0",
        "pony": pp_dir / "bin_pony_o0",
        "cpp": pp_dir / "bin_cpp",
    }

    with temporary_directories(*bin_dirs.values()):
        # Compile
        compile_coherence(config.compiler, pp_coh, bin_dirs["coh"], optimize=False)
        compile_pony(config.ponyc, pp_pony, bin_dirs["pony"], release=False)
        compile_cpp(pp_cpp, bin_dirs["cpp"] / "ping_pong")

        # Time executions
        t_pp_coh,  e_pp_coh  = time_exe([str(bin_dirs["coh"] / "out")])
        t_pp_pony, e_pp_pony = time_exe([str(bin_dirs["pony"] / "main"), "--ponymaxthreads", "16"])
        t_pp_cpp,  e_pp_cpp  = time_exe([str(bin_dirs["cpp"] / "ping_pong")])

        print(f"  Results: coh={t_pp_coh:.3f}±{e_pp_coh:.3f}s, pony={t_pp_pony:.3f}±{e_pp_pony:.3f}s, cpp={t_pp_cpp:.3f}±{e_pp_cpp:.3f}s")

        # Plot
        labels   = ['Coherence', 'Pony', 'C++']
        times    = [t_pp_coh, t_pp_pony, t_pp_cpp]
        errors   = [e_pp_coh, e_pp_pony, e_pp_cpp]
        colors   = ['#0000FF', '#00FF00', '#FF0000']

        plt.figure(figsize=(10, 6))
        bars = plt.bar(labels, times, yerr=errors, color=colors, width=0.5,
                       capsize=12, error_kw={"elinewidth": 3, "ecolor": "#FFD700", "capthick": 3})

        # Annotate bars with exact times
        for bar, t, e in zip(bars, times, errors):
            plt.text(
                bar.get_x() + bar.get_width() / 2,
                bar.get_height() + e + 0.05,
                f'{t:.2f}s',
                ha='center', va='bottom', fontsize=11, fontweight='bold'
            )

        # Legend entry for error bars
        from matplotlib.lines import Line2D
        plt.legend(
            handles=[Line2D([0], [0], color="#FFD000AA", linewidth=1.5, label='Error bars (±1 std dev)')],
            loc='upper left'
        )

        plt.ylabel('Time (s)')
        plt.title('Ping Pong Benchmark\n(n=1,000 actors, m=100 pings each)')
        plt.grid(axis='y', linestyle='--', alpha=0.5)
        plt.tight_layout()
        plt.savefig(config.output_dir / "ping_pong_report.png")
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

            elapsed, _ = time_compilation(cmd)
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
    
    benchmark_ping_pong(config)
    benchmark_compilation_time(config)
    sys.exit(0)


if __name__ == "__main__":
    main()