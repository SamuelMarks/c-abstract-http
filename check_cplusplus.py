import os

for root, dirs, files in os.walk('.'):
    if 'vcpkg' in root or '_deps' in root or 'build' in root or 'sdl' in root or 'c-rest-framework' in root or 'cdd_test_helpers' in root:
        continue
    for file in files:
        if file.endswith('.h'):
            filepath = os.path.join(root, file)
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            count = content.count('#ifdef __cplusplus')
            if count == 0:
                print(f"Missing __cplusplus guard: {filepath}")
            elif count > 2:
                print(f"Multiple __cplusplus guards: {filepath} ({count})")
            elif count == 1:
                print(f"Only one __cplusplus guard: {filepath}")
