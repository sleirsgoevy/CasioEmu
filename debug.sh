emulator/bin/casioemu models/fx570esplus 2>dump.log #run the program
python3 ropdb.py <(python3 asm_annotate.py < test.asm | python3 ../compiler.py llh) <(python3 find_gadgets.py < dump.log) test.asm
