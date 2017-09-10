#include "../CPU.hpp"

#include "../Emulator.hpp"
#include "../Chipset.hpp"
#include "../MMU.hpp"

namespace casioemu
{
	// * PUSH/POP Instructions
	void CPU::OP_PUSH()
	{
		size_t push_size = impl_operands[1].register_size;
		if (push_size == 1)
			push_size = 2;
		reg_sp -= push_size;
		for (size_t ix = 0; ix != impl_operands[1].register_size; ++ix)
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
			Push16(reg_elr[reg_psw & PSW_ELEVEL]);
		if (impl_operands[1].value & 1)
			Push16(reg_ea);
	}

	void CPU::OP_POP()
	{
		size_t pop_size = impl_operands[0].register_size;
		if (pop_size == 1)
			pop_size = 2;
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
			reg_lr = Pop16();
		if (impl_operands[0].value & 4)
			reg_psw = Pop16();
		if (impl_operands[0].value & 2)
		{
			reg_pc = Pop16();
			if (memory_model == MM_LARGE)
				reg_csr = Pop16() & 0x000F;
		}
	}

	void CPU::Push16(uint16_t data)
	{
		reg_sp -= 2;
		emulator.chipset.mmu.WriteData(reg_sp, data & 0xFF);
		emulator.chipset.mmu.WriteData(reg_sp + 1, data >> 8);
	}

	uint16_t CPU::Pop16()
	{
		uint16_t result = emulator.chipset.mmu.ReadData(reg_sp) | (((uint16_t)emulator.chipset.mmu.ReadData(reg_sp + 1)) << 8);
		reg_sp += 2;
		return result;
	}
}

