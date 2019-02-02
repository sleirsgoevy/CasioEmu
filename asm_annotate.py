i = 1

while True:
    try: l = input()
    except EOFError: break
    print('__line_%d:'%i)
    print(l)
    i += 1
