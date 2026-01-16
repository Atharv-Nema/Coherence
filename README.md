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
- `boost`

On Ubuntu, for example:

```sh
sudo apt update
sudo apt install build-essential cmake flex bison
sudo apt install libboost-all-dev
sudo apt-get install nlohmann-json3-dev
```

## Build Instructions

From the root of the repository, follow these steps:

---

1.  **Create a build directory:**

    ```sh
    mkdir build
    ```



2.  **Generate the build files with CMake:**

    ```sh
    cmake -S . -B build
    ```


3.  **Change into the build directory:**

    ```sh
    cd build
    ```

4.  **Build the project:**

    ```sh
    make
    ```

5. **Exit the build directory**
    ```sh
    cd ../
    ```

6. **Run the compiler on a source file:**

    ```sh
    ./build/compiler/coherence --input-file=./sample_programs/well_typed_programs/hello_world_equivalent.coh --output-dir=temp
    ```

    This runs the full compiler pipeline and emits all generated artifacts into the specified output directory (`temp`).

    You can then execute the generated program:

    ```sh
    ./temp/out
    ```

    This should print:

    ```
    42
    ```
