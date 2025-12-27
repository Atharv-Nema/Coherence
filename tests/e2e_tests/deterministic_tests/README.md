# Deterministic Tests

A **deterministic test** is represented as a **single directory** containing the program and the expected outputs.

## Directory Layout

Each test directory must contain:

1. **A `prog.coh` file**  
   - The Coherence source program to compile and run.

2. **`compiler_out.txt`**  
   - The expected output produced by the **compiler invocation**.

3. **`prog_output.txt`**  
   - The expected output produced when running the **compiled program**.