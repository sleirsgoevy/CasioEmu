import sys

addrs = [int(i, 16) for i in sys.argv[1:]]

while True:
    x = input()
    if x.startswith('[ram] ') and any(x.endswith(' %d'%i) for i in addrs):
        print('[mark] 0x%x: %s'%([i for i in addrs if x.endswith(' %d'%i)][0], x))
    print(x)
