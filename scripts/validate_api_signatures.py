import os
import subprocess
import sys
import re


def main():
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}

    print("--> Building Compilation Database...")
    res = subprocess.run(
        [
            "cmake",
            "-B",
            "build-strict",
            "-S",
            ".",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ],
        env=env,
    )
    if res.returncode != 0:
        print("Failed to configure cmake.")
        sys.exit(1)

    print("--> Building and Testing...")
    res = subprocess.run(["cmake", "--build", "build-strict"], env=env)
    if res.returncode != 0:
        print("Failed to build project.")
        sys.exit(1)

    res = subprocess.run(
        ["ctest", "-C", "Debug", "--output-on-failure"], cwd="build-strict", env=env
    )
    if res.returncode != 0:
        print("Tests failed.")
        sys.exit(1)

    print("--> Checking API signatures for non error enum functions...")
    has_error = False
    # Regex to catch API functions returning int or raw enum c_abstract_http_error instead of c_abstract_http_error_t
    regex = re.compile(
        r"^\s*(extern\s+)?int\s+(http_|c_abstract_http_)[a-zA-Z0-9_]*\s*\("
    )
    raw_enum_regex = re.compile(r"\benum\s+c_abstract_http_error\b")
    comment_regex = re.compile(r"/\*.*?\*/", re.DOTALL)
    usage_regex = re.compile(
        r"([^;{}=,()]{0,120})\bc_abstract_http_error_t\s*(\(\*|[a-zA-Z_][a-zA-Z0-9_]*\s*\()"
    )
    api_fn_regex = re.compile(
        r"([^;{}]{0,80})\bC_ABSTRACT_HTTP_API\s+([a-zA-Z_][a-zA-Z0-9_\s\*]*?)\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\("
    )

    http_types_path = os.path.join(
        "include", "c_abstract_http", "http_types.h"
    )
    if os.path.exists(http_types_path):
        with open(http_types_path, "r") as f:
            http_types_content = f.read()
        if "} c_abstract_http_error_t NO_DISCARD;" not in http_types_content:
            print(
                f"{http_types_path}: c_abstract_http_error_t typedef missing NO_DISCARD"
            )
            has_error = True

    for root, _, files in os.walk("include"):
        for file in files:
            if file.endswith(".h"):
                filepath = os.path.join(root, file)
                with open(filepath, "r") as f:
                    raw = f.read()
                for line_idx, line in enumerate(raw.splitlines()):
                    if regex.search(line):
                        print(f"{filepath}:{line_idx + 1}: {line.strip()}")
                        has_error = True
                    if (
                        raw_enum_regex.search(line)
                        and "typedef enum" not in line
                    ):
                        print(f"{filepath}:{line_idx + 1}: {line.strip()}")
                        has_error = True
                clean = comment_regex.sub(
                    lambda m: "\n" * m.group(0).count("\n"), raw
                )
                for m in usage_regex.finditer(clean):
                    if "NO_DISCARD" not in m.group(1):
                        line_no = clean[: m.start()].count("\n") + 1
                        snippet = " ".join(
                            clean[m.start() : m.end() + 30].split()
                        )
                        print(
                            f"{filepath}:{line_no}: missing NO_DISCARD on {snippet}"
                        )
                        has_error = True
                for m in api_fn_regex.finditer(clean):
                    ret_type = " ".join(m.group(2).split())
                    fn_name = m.group(3)
                    if ret_type not in ("c_abstract_http_error_t", "void") and not fn_name.startswith("math_"):
                        line_no = clean[: m.start()].count("\n") + 1
                        print(
                            f"{filepath}:{line_no}: API function '{fn_name}' returns '{ret_type}' instead of 'c_abstract_http_error_t' or 'void'"
                        )
                        has_error = True

    if has_error:
        print(
            "ERROR: Found invalid API signatures (must use 'c_abstract_http_error_t' with NO_DISCARD)."
        )
        sys.exit(1)

    print("All checks passed.")


if __name__ == "__main__":
    main()
