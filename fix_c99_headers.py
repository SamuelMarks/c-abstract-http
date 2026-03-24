import os

stdint_replacement = """#if defined(_MSC_VER) && _MSC_VER < 1600
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif"""

unistd_replacement = """#if !defined(_MSC_VER)
#include <unistd.h>
#endif"""

stdbool_replacement = """#if defined(_MSC_VER) && _MSC_VER < 1800 && !defined(__cplusplus)
#ifndef bool
typedef unsigned char bool;
#define true 1
#define false 0
#endif
#else
#include <stdbool.h>
#endif"""

def process_file(filepath):
    with open(filepath, 'rb') as f:
        content = f.read().decode('utf-8')

    orig_content = content
    content = content.replace('#include <stdint.h>', stdint_replacement)
    content = content.replace('#include <unistd.h>', unistd_replacement)
    content = content.replace('#include <stdbool.h>', stdbool_replacement)
    
    if orig_content != content:
        with open(filepath, 'wb') as f:
            f.write(content.encode('utf-8'))

for d in ['src', 'include', 'tests']:
    for root, dirs, files in os.walk(d):
        for file in files:
            if file.endswith('.c') or file.endswith('.h'):
                filepath = os.path.join(root, file)
                process_file(filepath)
