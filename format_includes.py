import os
import re

def process_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # Remove all existing clang-format comments
    content = re.sub(r'/\*\s*clang-format\s+(off|on)\s*\*/\n?', '', content)
    lines = content.split('\n')
    
    # Find block of includes
    include_start = -1
    include_end = -1
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            if include_start == -1:
                include_start = i
            include_end = i
    
    if include_start != -1:
        # Expand start upwards to include any #if / #ifdef
        j = include_start - 1
        while j >= 0 and lines[j].strip().startswith('#'):
            if lines[j].strip().startswith(('#ifndef', '#define')):
                # Check if it's an include guard
                if '_H' in lines[j] or 'TEST_' in lines[j] or 'C_ABSTRACT_HTTP' in lines[j]:
                    break
            include_start = j
            j -= 1

        # Expand end downwards to include any #endif
        j = include_end + 1
        while j < len(lines) and lines[j].strip().startswith('#'):
            if lines[j].strip().startswith(('#endif', '#else', '#elif')):
                include_end = j
            else:
                break
            j += 1

        new_lines = []
        for i, line in enumerate(lines):
            if i == include_start:
                new_lines.append('/* clang-format off */')
            new_lines.append(line)
            if i == include_end:
                new_lines.append('/* clang-format on */')
                
        content = '\n'.join(new_lines)
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)

for d in ['src', 'include', 'tests']:
    for root, dirs, files in os.walk(d):
        for file in files:
            if file.endswith('.c') or file.endswith('.h'):
                filepath = os.path.join(root, file)
                process_file(filepath)
