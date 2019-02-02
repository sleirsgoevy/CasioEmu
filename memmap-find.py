from collections import OrderedDict
import sys

mem_only = False

if '--chrono' in sys.argv:
    the_dict = OrderedDict
    the_sorted = lambda x: x
else:
    the_dict = dict
    the_sorted = sorted

if '--mem-only' in sys.argv:
    mem_only = True

old_data = []

data = the_dict()

while True:
    try: l = input()
    except EOFError: break
    if l == '[cpu] cpu reset':
        old_data.append(data)
        data = the_dict()
    elif l.startswith('[ram] read byte '):
        l = l.split()
        addr = int(l[-1])
        if mem_only and addr not in range(0x8000, 0x8e00): continue
        val = int(l[-3])
        if addr not in data: data[addr] = val
    elif l.startswith('[ram] write byte '):
        addr = int(l.split()[-1])
        if mem_only and addr not in range(0x8000, 0x8e00): continue
        if addr not in data: data[addr] = None

old_data.append(data)

for i in old_data:
    for k, v in the_sorted(i.items()):
        if v != None:
            print('[0x%x] = %d'%(k, v))
    print('#'*50)
