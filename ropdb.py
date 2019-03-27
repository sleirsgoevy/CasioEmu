regs = {i: 0 for i in range(16)}
mem = {}

class ReprInt(int):
    def __new__(self, i, r):
        self = int.__new__(self, i)
        self.r = r
        return self
    def __repr__(self):
        return self.r
    def __eq__(self, other):
        if isinstance(other, ReprInt):
            return self.r == other.r
        return int.__eq__(self, other)
    def __hash__(self):
        return hash(int(self))

class MemoryAccessor:
    def __init__(self, mem, sz):
        self.mem = mem
        self.sz = sz
    def __getitem__(self, addr):
        val = ''
        for i in range(addr, addr + self.sz):
            if i in self.mem: val = '%02x' % self.mem[i] + val
            else: val = '??' + val
        num = int.from_bytes(bytes.fromhex(val.replace('?', '0')), 'big', signed=True)
        while len(val) > 1 and val[0] == '0': val = val[1:]
        return ReprInt(num, '0x%s (%s)'%(val, '?' if '?' in val else num))
    __pow__ = __getitem__

class RegsAccessor:
    def __init__(self, regs):
        self.mems = {'r': MemoryAccessor(regs, 1), 'er': MemoryAccessor(regs, 2), 'xr': MemoryAccessor(regs, 4), 'qr': MemoryAccessor(regs, 8)}
    def __getattr__(self, reg):
        for k, v in self.mems.items():
            if reg.startswith(k) and reg != k and reg[len(k):].isnumeric():
                regno = int(reg[len(k):])
                if regno % v.sz: continue
                return v[regno]
        raise AttributeError(reg)

class History(list):
    __slots__ = ()
    def __pow__(self, x):
        return self[x - 1]

regs_accessor = RegsAccessor(regs)
mem8 = MemoryAccessor(mem, 1)
mem16 = MemoryAccessor(mem, 2)
mem32 = MemoryAccessor(mem, 4)
mem64 = MemoryAccessor(mem, 8)

mode = 'step'
reverse = False
breaks = {}
watches = {}
watch_values = {}
disabled_breaks = set()

import sys

if len(sys.argv) > 2 and sys.argv[1] == '--disas':
    with open(sys.argv[2]) as file:
        disas = file.read().split('\n')
    del sys.argv[1:3]
else:
    disas = []

disas_by_pc = {}
for l in disas:
    if '; ' in l:
        a, b = l.split('; ', 1)
        b = b.split()[0]
        try: disas_by_pc[int(b, 16)] = a.strip()
        except ValueError: pass

if len(sys.argv) not in (3, 4):
    print('Usage: ropdb [--disas <disasm_file>] <compiled program> <emulator dump> [source_file]')
    exit(1)

if len(sys.argv) == 4:
    source = open(sys.argv[3]).read().split('\n')
else:
    source = None

prog = open(sys.argv[1]).read()

labels = {}
min_label = float('inf')
max_label = float('-inf')
have_debug = False

for i in prog.split('\n'):
    if '=' in i:
        a, b = i.strip().split('=')
        a = a.strip()
        b = b.strip()
        if a and b:
            try: b = int(b, 16)
            except ValueError: continue
            labels[a] = b
            if a.startswith('__line_'):
                have_debug = True
            min_label = min(min_label, b)
            max_label = max(max_label, b)

if not have_debug:
    print('Warning: no line information present. Use `asm_annotate.py` to add it.')

dump = open(sys.argv[2]).read().split('\n')
dump_idx = -1

mem_writes = []
regs_stack = []
history = History()

def format_code(s):
    s = s.split('$')
    for i, si in enumerate(s[1:]):
        if si[:1].isnumeric():
            s[i + 1] = 'history**'+s[i - 1]
        else:
            s[i + 1] = 'regs_accessor.'+s[i - 1]
    return ''.join(s).replace('{char}', 'mem8**').replace('{short}', 'mem16**').replace('{int}', 'mem32**').replace('{int64}', 'mem64**')

def format_ptr(ptr):
    ptr &= 65535
    ans = '0x%04x' % ptr
    for k, v in labels.items():
        if v == ptr:
            if k.startswith('__line_'):
                ans += ' (line %s)'%k[7:]
            else:
                ans += ' [%s]'%k
    return ans

locals = {
    'char': lambda x: (x + 128 & 255) - 128,
    'short': lambda x: (x + 32768 & 65535) - 32768,
    'int': lambda x: (x + (1 << 31) & ((1 << 32) - 1)) - (1 << 31),
    'ptr': lambda x: ReprInt(x, format_ptr(x))
}

for k, v in labels.items():
    if k.startswith('__line_'):
        locals[k] = ReprInt(v, '0x%04x (line %s)'%(v, k[7:]))
    else:
        locals[k] = ReprInt(v, '0x%04x [%s]'%(v, k))

def eval_safe(s):
    try: return eval(s, globals(), locals)
    except BaseException as e: return e

while True:
    if reverse:
        dump_idx -= 1
        if dump_idx < 0:
            print('Reached the beginning, going forward')
            dump_idx = 0
            reverse = False
    else:
        dump_idx += 1
        if dump_idx >= len(dump):
            print('Reached the end, exiting')
            break
    for i, j in watches.items():
        val = eval_safe(j)
        if i in watch_values and watch_values[i] != val and i not in disabled_breaks:
            print('Watchpoint %d: old value %r, new %r'%(i, watch_values[i], val))
            mode = 'step'
        watch_values[i] = val
    l = dump[dump_idx].strip()
    if l.startswith('[trace] regs = '):
        if reverse:
            assert regs_stack.pop() == l
            l = regs_stack[-1] if regs_stack else ('[trace] regs ='+' 00'*16)
        else:
            regs_stack.append(l)
        for i, v in enumerate(l.split()[3:]):
            regs[i] = int(v, 16)
    elif l.startswith('[ram] write byte '):
        if reverse:
            k, v = mem_writes.pop()
            if v == None: del mem[k]
            else: mem[k] = v
        else:
            v = int(l.split()[3])
            k = int(l.split()[5])
            mem_writes.append((k, mem.get(k, None)))
            mem[k] = v
    elif l.startswith('[rop] pop pc ') or (l.startswith('[trace] pc=') and mode[-1:] == 'i'):
        sp_pos = int(l.split()[5][3:], 16) if l.startswith('[rop] pop pc') else regs_accessor.sp
        have_break = False
        for k, v in breaks.items():
            if v == sp_pos and k not in disabled_breaks:
                print('Breakpoint', k)
                have_break = True
        legend = 'at 0x%04x'%sp_pos
        have_label = False
        lineno = None
        for k, v in labels.items():
            if v == sp_pos:
                if k.startswith('__line_'):
                    legend += ' (line %s)'%k[7:]
                    try: lineno = int(k[7:])
                    except ValueError: pass
                else:
                    legend += ' [%s]'%k
                have_label = True
        if l.startswith('[rop] pop pc ') and '#gadgets:' in l:
            gadget = l.split('#gadgets:', 1)[1].strip()
        else:
            gadget = None
        if not have_break and ((lineno == None and gadget == None and mode[-1:] != 'i') or mode == 'cont'): continue
        if not have_label:
            legend += ' [??]'
            if max_label - min_label <= 100:
                for k, v in labels.items():
                    if (v - sp_pos) % 100 == 0:
                        legend += ' [probably '+k+']'
        i = dump_idx
        pc = int(l.split()[3], 16) if l.startswith('[rop] pop pc ') else 0
        have_msb = False
        while i < len(dump) and not dump[i].startswith('[trace] pc='):
            i += 1
        if i < len(dump):
            pc = int(dump[i][11:], 16)
            have_msb = True
        pc = ReprInt(pc, ('0x%05x' if have_msb else '0x?%04x')%pc)
        regs_accessor.pc = pc
        regs_accessor.sp = ReprInt(sp_pos, legend[3:])
        legend += ' pc=%r'%pc
        if l.startswith('[trace] pc='):
            legend += ' (singlestep)'
        elif gadget == None:
            legend += ' (no known gadget)'
        print(legend)
        if lineno != None and source != None and lineno - 1 in range(len(source)):
            gadget2 = source[lineno - 1]
            if gadget != None:
                gadget3 = gadget.split('{')
                gadget3 = gadget3[0]+''.join(i.split('}', 1)[1] for i in gadget3[1:])
                def format_gadget(g):
                    g = g.lower().split()
                    for i in range(len(g)):
                        if g[i][:1].isalnum() or g[i][:1] == '_': g[i] = ' ' + g[i]
                        if g[i][-1:].isalnum() or g[i][-1:] == '_': g[i] += ' '
                    return ' '.join(''.join(g).split())
                if format_gadget(gadget3) != format_gadget(gadget2):
                    print('Warning: actual gadget is different:', gadget.strip())
            gadget = gadget2
        if gadget:
            if l.startswith('[trace] pc='): print('    in gadget: '+gadget)
            else: print('    '+gadget)
        if l.startswith('[trace] pc='):
            if pc in disas_by_pc: print('    disas: '+disas_by_pc[pc])
        import readline
        while True:
            try: cmd = input('(ropdb) ')
            except EOFError: exit()
            cmd_spl = cmd.split()
            py_code = cmd[cmd.find(cmd_spl[1], cmd.find(cmd_spl[0]) + len(cmd_spl[0])):] if len(cmd_spl) >= 2 else None
            if not cmd_spl: break
            elif cmd_spl[0] == 'break':
                break_no = len(breaks) + len(watches) + 1
                if py_code.strip().isnumeric():
                    if not have_debug:
                        print('No line number information')
                        continue
                    else:
                        try: sp = labels['__line_'+str(int(py_code))]
                        except KeyError:
                            print('Line %d not found'%int(py_code))
                            continue
                else:
                    if py_code.startswith('*'): py_code = py_code[1:]
                    if py_code.strip():
                        try: sp = eval(format_code(py_code), globals(), locals) & 65535
                        except:
                            import traceback
                            traceback.print_exc()
                            continue
                    else:
                        sp = sp_pos
                breaks[break_no] = sp
                print('Breakpoint %d at 0x%04x'%(break_no, breaks[break_no]))
            elif cmd_spl[0] == 'watch':
                if not py_code:
                    print('usage: watch <expression>')
                    continue
                watch_no = len(breaks) + len(watches) + 1
                watches[watch_no] = format_code(py_code)
                watch_values[watch_no] = eval_safe(watches[watch_no])
                print('Watchpoint %d: %s'%(watch_no, py_code))
            elif cmd_spl[0] == 'enable':
                if len(cmd_spl) != 2 or not cmd_spl[1].isnumeric():
                    print('usage: enable <break_no>')
                    continue
                try: disabled_breaks.remove(int(cmd_spl[1]))
                except KeyError: pass
            elif cmd_spl[0] == 'disable':
                if len(cmd_spl) != 2 or not cmd_spl[1].isnumeric():
                    print('usage: disable <break_no>')
                    continue
                disabled_breaks.add(int(cmd_spl[1]))
            elif cmd_spl[0] in ('step', 'next', 'cont', 'reverse-step', 'reverse-next', 'reverse-cont', 'stepi', 'nexti', 'reverse-stepi', 'reverse-nexti'):
                if len(cmd_spl) != 1:
                    print('usage: '+cmd_spl[0])
                    continue
                reverse = False
                if cmd_spl[0].startswith('reverse-'):
                    reverse = True
                    cmd_spl[0] = cmd_spl[0][8:]
                if cmd_spl[0][:4] == 'next': cmd_spl[0] = 'step' + cmd_spl[0][4:]
                mode = cmd_spl[0]
                break
            elif cmd_spl[0] == 'l':
                if len(cmd_spl) == 2:
                    try: lineno = int(cmd_spl[1])
                    except ValueError:
                        print('Not a number:', cmd_spl[1].strip())
                        continue
                elif len(cmd_spl) != 1:
                    print('usage: l [lineno]')
                    continue
                if source == None or lineno not in range(len(source)):
                    print('Source code not available')
                    continue
                for i in range(max(0, lineno - 4), min(len(source), lineno + 5)):
                    print(i + 1, source[i].strip())
                lineno += 10
            elif cmd_spl[0] == 'less_from_here':
                if len(cmd_spl) != 1:
                    print('usage: less_from_here')
                    continue
                import pydoc
                data = '\n'.join(dump[max(0, dump_idx-4):dump_idx]+['--> '+dump[dump_idx]]+dump[dump_idx+1:])
                print(repr(data[:500]))
                pydoc.pager(data)
            elif cmd.startswith('!'):
                import os
                os.system(cmd[1:])
            else:
                if cmd_spl[0] != 'p': py_code = cmd
                try:
                    val = eval(format_code(py_code), globals(), locals)
                    history.append(val)
                    print('$%d = %r'%(len(history), val))
                except:
                    import traceback
                    traceback.print_exc()
                    continue

