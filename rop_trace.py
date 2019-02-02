regs = 'unknown'

while True:
    try: l = input()
    except EOFError: break
    if l.startswith('[trace] regs = '):
        regs = l[15:]
    elif '#gadgets: ' in l:
        print('%-80s #regs = %s'%(l.split('#gadgets: ', 1)[1], regs))
