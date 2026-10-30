import os


def get_project_root() -> str:
    """
    Resolve the NKFADC500_mini_CLI repository root deterministically,
    regardless of whether the GUI was launched via `python3 gui/main.py`,
    `./bin/fadc500_gui`, or a symlinked `bin/gui/` copy.
    """
    env_root = os.environ.get("NKFADC500_ROOT", "").strip()
    if env_root and os.path.isdir(env_root) and os.path.exists(os.path.join(env_root, "CMakeLists.txt")):
        return os.path.abspath(env_root)

    curr = os.path.dirname(os.path.abspath(__file__))
    for _ in range(6):
        if os.path.exists(os.path.join(curr, "CMakeLists.txt")) and os.path.isdir(os.path.join(curr, "app")):
            return curr
        parent = os.path.dirname(curr)
        if parent == curr:
            break
        curr = parent

    # Fallback: two levels above gui/core
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))


def get_bin_path(exe_name: str) -> str:
    return os.path.join(get_project_root(), "bin", exe_name)


def get_default_data_dir() -> str:
    return os.path.join(get_project_root(), "data")


def get_default_config_dir() -> str:
    return os.path.join(get_project_root(), "config")
