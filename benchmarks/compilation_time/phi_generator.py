from pathlib import Path

def get_func_str(n: int):
    return f"""
    func f{n}() => unit {{
        atomic {{
            {f"f{n}" if n != 0 else ""}
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
    new create(env: Env) {
        // Entry point logic if needed
    }
}
"""
        f.write(main_actor)