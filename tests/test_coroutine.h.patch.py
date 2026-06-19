import re

with open('tests/test_coroutine.h', 'r') as f:
    text = f.read()

# We need to see what's missing in coroutine.c. Lines: 254,291-292,297-299,349,372,377
