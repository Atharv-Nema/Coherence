# Coherence
An actor based programming language that is data-race and deadlock free

# Quick Setup

Follow these steps to build and run the Coherence compiler.

## Prerequisites

Make sure you have the following installed:

- A C++20 compiler (e.g. `g++` or `clang++`)
- `cmake`
- `make`
- `flex`
- `bison`

On Ubuntu, for example:

```sh
sudo apt update
sudo apt install build-essential cmake flex bison
```

## Build Instructions

From the root of the repository, follow these steps:

---

1.  **Create a build directory:**

    ```sh
    mkdir build
    ```

2.  **Change into the build directory:**

    ```sh
    cd build
    ```

3.  **Generate the build files with CMake:**

    ```sh
    cmake ..
    ```

4.  **Build the project:**

    ```sh
    make
    ```

5.  **Run the compiler on a source file:**

    ```sh
    ./compiler/coherence ../sample_programs/simple.coh
    ```
    This should not work, as the compiler is not ready yet. <br>
    To only run the type checker, run it with the <code> --only-typecheck</code> flag
    ```sh
    ./compiler/coherence --only-typecheck ../sample_programs/well_typed_programs/simple.coh
    ```

---