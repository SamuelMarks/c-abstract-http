import glob
for file in glob.glob(r'C:\Users\samue\repos\win-cmake-multi-build\*.bat'):
    with open(file, 'r', encoding='utf-8') as f:
        c = f.read()
    c = c.replace('call \\\"', 'call \"')
    with open(file, 'w', encoding='utf-8') as f:
        f.write(c)
