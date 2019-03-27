stack = []
fixed = 0

while True:
    l = input()
    if l.startswith('[mark] '):
        while fixed != len(stack):
            print('\t'*fixed+'call %08x'%stack[fixed])
            fixed += 1
        print('\t'*fixed+l)
    elif 'mark' in l: assert False, l
    elif l.startswith('[trace] call '):
        stack.append(int(l.split()[2], 16))
    elif l.startswith('[trace] return to '):
        try: it = stack.pop()
        except IndexError:
            print('wtf? empty stack!', l)
            continue
        if fixed > len(stack):
            fixed -= 1
            print('\t'*fixed+l[8:])
