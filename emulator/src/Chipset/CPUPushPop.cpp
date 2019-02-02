#include "CPU.hpp"

#include "../Emulator.hpp"
#include "Chipset.hpp"
#include "MMU.hpp"

#include <stdio.h>

namespace casioemu
{
	// * PUSH/POP Instructions
	void CPU::OP_PUSH()
	{
		size_t push_size = impl_operands[1].register_size;
       		if (push_size == 1)
			push_size = 2;
		fprintf(stderr, "[rop] push %d to %04x at %04x\n", (int)push_size, (int)(uint16_t)reg_sp, (int)(uint16_t)reg_pc);
		reg_sp -= push_size;
		for (size_t ix = impl_operands[1].register_size - 1; ix != (size_t)-1; --ix)
			emulator.chipset.mmu.WriteData(reg_sp + ix, impl_operands[1].value >> (8 * ix));
	}

	void CPU::OP_PUSHL()
	{
		if (impl_operands[1].value & 2)
		{
			if (memory_model == MM_LARGE)
				Push16(reg_ecsr[reg_psw & PSW_ELEVEL]);
			Push16(reg_elr[reg_psw & PSW_ELEVEL]);
		}
		if (impl_operands[1].value & 4)
			Push16(reg_epsw[reg_psw & PSW_ELEVEL]);
		if (impl_operands[1].value & 8)
		{
			if (memory_model == MM_LARGE)
				Push16(reg_lcsr);
			Push16(reg_lr);

			if (stack.empty())
				logger::Info("PUSH LR is called before %06zX, but the stack is empty\n",
						((size_t)reg_csr.raw) << 16 | reg_pc.raw);
			else if (stack.back().lr_pushed)
				logger::Info("PUSH LR is called before %06zX, but LR is already pushed for the last frame\n",
						((size_t)reg_csr.raw) << 16 | reg_pc.raw);
			else
			{
				stack.back().lr_pushed = true;
				stack.back().lr_push_address = reg_sp;
			}
		}
		if (impl_operands[1].value & 1)
			Push16(reg_ea);
	}

	void CPU::OP_POP()
	{
		size_t pop_size = impl_operands[0].register_size;
		if (pop_size == 1)
			pop_size = 2;
                fprintf(stderr, "[rop] pop %d from %04x at %04x\n", (int)pop_size, (int)(uint16_t)reg_sp, (int)(uint16_t)reg_pc);
		impl_operands[0].value = 0;
		for (size_t ix = 0; ix != impl_operands[0].register_size; ++ix)
			impl_operands[0].value |= ((uint64_t)emulator.chipset.mmu.ReadData(reg_sp + ix)) << (8 * ix);
		reg_sp += pop_size;
	}

	void CPU::OP_POPL()
	{
		if (impl_operands[0].value & 1)
			reg_ea = Pop16();
		if (impl_operands[0].value & 8)
		{
			/**
			 * Sometimes a function calls another function in one branch, and
			 * does not call another function in another branch. In that case
			 * the compiler may decide to do a `push lr` / `pop lr` in only the
			 * branch that has to save `lr`.
			 */
			if (!stack.empty() && stack.back().lr_pushed &&
					stack.back().lr_push_address == reg_sp)
				stack.back().lr_pushed = false;

			reg_lr = Pop16();
			if (memory_model == MM_LARGE)
				reg_lcsr = Pop16() & 0x000F;
		}
		if (impl_operands[0].value & 4)
			reg_psw = Pop16();
		if (impl_operands[0].value & 2)
		{
			if (!stack.empty() && stack.back().lr_pushed &&
					stack.back().lr_push_address == reg_sp)
				stack.pop_back();

			reg_pc = Pop16();
                        fprintf(stderr, "[rop] pop pc %04x at sp=%04x\n", (int)reg_pc, (int)(reg_sp - 2));
			if (memory_model == MM_LARGE)
				reg_csr = Pop16() & 0x000F;
		}
	}

	void CPU::Push16(uint16_t data)
	{
		reg_sp -= 2;
		emulator.chipset.mmu.WriteData(reg_sp + 1, data >> 8);
		emulator.chipset.mmu.WriteData(reg_sp, data & 0xFF);
	}

	uint16_t CPU::Pop16()
	{
		uint16_t result = emulator.chipset.mmu.ReadData(reg_sp) | (((uint16_t)emulator.chipset.mmu.ReadData(reg_sp + 1)) << 8);
		reg_sp += 2;
		return result;
	}
}

