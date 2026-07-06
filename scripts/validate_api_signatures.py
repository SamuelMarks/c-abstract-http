import os
import subprocess
import sys
import re

def main():
    env = os.environ.copy()
    for k in ["GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX"]:
        env.pop(k, None)

    print("--> Building Compilation Database...")
    res = subprocess.run(["cmake", "-B", "build", "-S", ".", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"], env=env)
    if res.returncode != 0:
        print("Failed to configure cmake.")
        sys.exit(1)

    print("--> Building and Testing...")
    res = subprocess.run(["cmake", "--build", "build"], env=env)
    if res.returncode != 0:
        print("Failed to build project.")
        sys.exit(1)

    res = subprocess.run(["ctest", "--output-on-failure"], cwd="build", env=env)
    if res.returncode != 0:
        print("Tests failed.")
        sys.exit(1)

    print("--> Checking API signatures for non error enum functions...")
    has_error = False
    # Regex to catch API functions returning int instead of enum c_abstract_http_error
    regex = re.compile(r'^\s*(extern\s+)?int\s+(http_|c_abstract_http_)[a-zA-Z0-9_]*\s*\(')

    for root, _, files in os.walk('include'):
        for file in files:
            if file.endswith('.h'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r') as f:
                    for line_idx, line in enumerate(f):
                        if regex.search(line):
                            print(f"{filepath}:{line_idx+1}: {line.strip()}")
                            has_error = True

    if has_error:
        print("ERROR: Found invalid API functions returning 'int' instead of 'enum c_abstract_http_error'.")
        sys.exit(1)

    print("All checks passed.")

if __name__ == "__main__":
    main()
