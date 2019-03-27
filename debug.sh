DUMP_LOG="$1"
ASM_FILE="$2"
NOTRUN="$3"

PYTHON=python3

if which pypy3
then PYTHON=pypy3
fi

if [ "x$NOTRUN" == x ]
then emulator/bin/casioemu models/fx570esplus 2>"$1" #run the program
fi
$PYTHON ropdb.py <($PYTHON asm_annotate.py < "$2" | python3 ../compiler.py -t loader2 -f hex) <($PYTHON find_gadgets.py < "$1") "$2"
