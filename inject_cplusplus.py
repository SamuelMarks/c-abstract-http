import os

def process_file(filepath):
    with open(filepath, 'rb') as f:
        content = f.read().decode('utf-8')

    if '#ifdef __cplusplus' in content:
        return # Already has it

    lines = content.split('\n')
    lines = [line.replace('\r', '') for line in lines]
    
    # Find the include guard #define
    define_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('#define ') and '_H' in line:
            define_idx = i
            break

    if define_idx == -1:
        print(f"No #define include guard found in {filepath}")
        return

    # Find the matching #endif at the end
    endif_idx = -1
    for i in range(len(lines)-1, -1, -1):
        if lines[i].startswith('#endif'):
            endif_idx = i
            break

    if endif_idx == -1:
        print(f"No #endif found in {filepath}")
        return

    # Insert extern C
    new_lines = lines[:define_idx+1]
    new_lines.append('')
    new_lines.append('#ifdef __cplusplus')
    new_lines.append('extern "C" {')
    new_lines.append('#endif /* __cplusplus */')
    
    new_lines.extend(lines[define_idx+1:endif_idx])

    new_lines.append('#ifdef __cplusplus')
    new_lines.append('}')
    new_lines.append('#endif /* __cplusplus */')
    new_lines.append(lines[endif_idx])

    with open(filepath, 'wb') as f:
        f.write('\n'.join(new_lines).encode('utf-8'))
    print(f"Processed {filepath}")

for d in ['include', 'tests', 'src']:
    for root, dirs, files in os.walk(d):
        for file in files:
            if file.endswith('.h'):
                filepath = os.path.join(root, file)
                process_file(filepath)
