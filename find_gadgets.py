#!/usr/bin/python3

data = {}

with open('../builtins') as file:
    contents = ('\n'+file.read()+'\n').split('\n/*\n')
    contents = contents[0] + ''.join(i.split('\n*/\n', 1)[1] for i in contents[1:])
    for line in contents.split('\n'):
        line = line.split('#', 1)[0].strip()
        if not line: continue
        l, g = line.split('\t', 1)
        data[int(l, 16) & -2] = g

while True:
    try: l = input()
    except EOFError: break
    if l.startswith('[rop] pop pc '):
        addr = int(l.split()[3], 16) & -2
        ls = [l]
        while not ls[-1].startswith('[trace] pc='):
            try: ls.append(input())
            except EOFError: break
        else:
            addr = int(ls[-1].split('=', 1)[1], 16) & -2
        if addr in data: ls[0] += ' #gadgets: ' + data[addr]
        for i in ls: print(i)
    else: print(l)
