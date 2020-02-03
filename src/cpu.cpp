//
// This 6502 emulator is based on https://github.com/gianlucag/mos6502
//	created by Gianluca Ghettini. Significant changes have been made
//	to support IceBro including cycle timing and architectural changes.
//	the original header file comment is included below. Any bugs or
//	errors were probably caused by me. If you are looking for 6502
//	simulation code, consider using mos6502 instead of this.
//
//============================================================================
// Name        : mos6502 
// Author      : Gianluca Ghettini
// Version     : 1.0
// Copyright   : 
// Description : A MOS 6502 CPU emulator written in C++
//============================================================================


#include "stdafx.h"
#include "cpu.h"

#define addr_nmi_l 0xfffa
#define addr_nmi_h 0xfffb
#define addr_reset_l 0xfffc
#define addr_reset_h 0xfffd
#define addr_irq_l 0xfffe
#define addr_irq_h 0xffff

class cpu
{
public:
	typedef void (cpu::*Instruction)(uint16_t);

	typedef struct Opcode {
		Instruction code;
		uint8_t time;
		AddressModes mode:8;
	} Opcode;

	// stack operations
	void Push(uint8_t byte);
	uint8_t Pop();

	// Manage status flags
	inline void SetFlag(uint8_t flag) { r.P |= flag; }
	inline void ClearFlag(uint8_t flag) { r.P &= ~flag; }
	inline bool CheckFlag(uint8_t flag) const { return !!(r.P & flag); }
	inline void SetFlag(bool condition, uint8_t flag) { if (condition) SetFlag(flag); else ClearFlag(flag); }
	inline void SetC(bool f) { SetFlag(f, F_C); }
	inline void EvlZ(bool f) { SetFlag(f, F_Z); }
	inline void EvlZ(uint8_t v) { SetFlag(!v, F_Z); }
	inline void EvlZ(uint16_t v) { SetFlag(!(v&0xff), F_Z); }
	inline void EvlN(uint8_t v) { SetFlag(!!(v&0x80), F_N); }
	inline void EvlN(uint16_t v) { SetFlag(!!(v&0x80), F_N); }
	inline bool ChkC() { return CheckFlag(F_C); }
	inline bool ChkZ() { return CheckFlag(F_Z); }
	inline bool ChkI() { return CheckFlag(F_I); }
	inline bool ChkD() { return CheckFlag(F_D); }
	inline bool ChkB() { return CheckFlag(F_B); }
	inline bool ChkU() { return CheckFlag(F_U); }
	inline bool ChkV() { return CheckFlag(F_V); }
	inline bool ChkN() { return CheckFlag(F_N); }

	// replaces direct addressing modes
	uint16_t GetArg(AddressModes mode);
	void ADC(uint16_t arg);
	void AND(uint16_t arg);
	void ASL(uint16_t arg);
	void ASLA(uint16_t arg);
	void BCC(uint16_t arg);
	void BCS(uint16_t arg);
	void BEQ(uint16_t arg);
	void BIT(uint16_t arg);
	void BMI(uint16_t arg);
	void BNE(uint16_t arg);
	void BPL(uint16_t arg);
	void BRK(uint16_t arg);
	void BVC(uint16_t arg);
	void BVS(uint16_t arg);
	void CLC(uint16_t arg);
	void CLD(uint16_t arg);
	void CLI(uint16_t arg);
	void CLV(uint16_t arg);
	void CMP(uint16_t arg);
	void CPX(uint16_t arg);
	void CPY(uint16_t arg);
	void DEC(uint16_t arg);
	void DEX(uint16_t arg);
	void DEY(uint16_t arg);
	void EOR(uint16_t arg);
	void INC(uint16_t arg);
	void INX(uint16_t arg);
	void INY(uint16_t arg);
	void JMP(uint16_t arg);
	void JSR(uint16_t arg);
	void LDA(uint16_t arg);
	void LDX(uint16_t arg);
	void LDY(uint16_t arg);
	void LSR(uint16_t arg);
	void LSRA(uint16_t arg);
	void NOP(uint16_t arg);
	void ORA(uint16_t arg);
	void PHA(uint16_t arg);
	void PHP(uint16_t arg);
	void PLA(uint16_t arg);
	void PLP(uint16_t arg);
	void ROL(uint16_t arg);
	void ROLA(uint16_t arg);
	void ROR(uint16_t arg);
	void RORA(uint16_t arg);
	void RTI(uint16_t arg);
	void RTS(uint16_t arg);
	void SBC(uint16_t arg);
	void SEC(uint16_t arg);
	void SED(uint16_t arg);
	void SEI(uint16_t arg);
	void STA(uint16_t arg);
	void STX(uint16_t arg);
	void STY(uint16_t arg);
	void TAX(uint16_t arg);
	void TAY(uint16_t arg);
	void TSX(uint16_t arg);
	void TXA(uint16_t arg);
	void TXS(uint16_t arg);
	void TYA(uint16_t arg);

	// invalid opcode

	void INV(uint16_t arg) { r.T = 0xff; }

	// illegal opcodes

	void SLO(uint16_t arg) { r.T = 0xff; }
	void ANC(uint16_t arg) { r.T = 0xff; }
	void RLA(uint16_t arg) { r.T = 0xff; }
	void AAC(uint16_t arg) { r.T = 0xff; }
	void ISC(uint16_t arg) { r.T = 0xff; }
	void SBI(uint16_t arg) { r.T = 0xff; }
	void DCP(uint16_t arg) { r.T = 0xff; }
	void SRE(uint16_t arg) { r.T = 0xff; }
	void ALR(uint16_t arg) { r.T = 0xff; }
	void RRA(uint16_t arg) { r.T = 0xff; }
	void ARR(uint16_t arg) { r.T = 0xff; }
	void SAX(uint16_t arg) { r.T = 0xff; }
	void LAX(uint16_t arg) { r.T = 0xff; }
	void AXS(uint16_t arg) { r.T = 0xff; }
	void LAS(uint16_t arg) { r.T = 0xff; }
	void AHX(uint16_t arg) { r.T = 0xff; }
	void SHX(uint16_t arg) { r.T = 0xff; }
	void TAS(uint16_t arg) { r.T = 0xff; }
	void SHY(uint16_t arg) { r.T = 0xff; }
	void XAA(uint16_t arg) { r.T = 0xff; }

	// Registers
	Regs r;

	// read/write callbacks
	CBGetByte GetByte;
	CBSetByte SetByte;

	// indirect page boundary crossed
	bool penalty;

	// set up class for stepping or setting status
	cpu(const Regs &regs, CBGetByte read, CBSetByte write) : penalty(false) {
		r = regs;
		GetByte = read;
		SetByte = write;
	}

	// actions
	Regs NMI();
	Regs IRQ();
	Regs Reset();
	Regs Step();
};

cpu::Opcode table[256] = {
	{ &cpu::BRK, 0x0e, AM_NON },
	{ &cpu::ORA, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SLO, 0xff, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ORA, 0x06, AM_ZP },
	{ &cpu::ASL, 0x0a, AM_ZP },
	{ &cpu::SLO, 0xff, AM_ZP },
	{ &cpu::PHP, 0x06, AM_NON },
	{ &cpu::ORA, 0x04, AM_IMM },
	{ &cpu::ASLA, 0x04, AM_NON },
	{ &cpu::ANC, 0xff, AM_IMM },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ORA, 0x08, AM_ABS },
	{ &cpu::ASL, 0x0c, AM_ABS },
	{ &cpu::SLO, 0xff, AM_ABS },
	{ &cpu::BPL, 0x05, AM_BRANCH },
	{ &cpu::ORA, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SLO, 0xff, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ORA, 0x08, AM_ZP_X },
	{ &cpu::ASL, 0x0c, AM_ZP_X },
	{ &cpu::SLO, 0xff, AM_ZP_X },
	{ &cpu::CLC, 0x04, AM_NON },
	{ &cpu::ORA, 0x09, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SLO, 0xff, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ORA, 0x09, AM_ABS_X },
	{ &cpu::ASL, 0x0e, AM_ABS_X },
	{ &cpu::SLO, 0xff, AM_ABS_X },
	{ &cpu::JSR, 0x0c, AM_ABS },
	{ &cpu::AND, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::RLA, 0xff, AM_ZP_REL_X },
	{ &cpu::BIT, 0x06, AM_ZP },
	{ &cpu::AND, 0x06, AM_ZP },
	{ &cpu::ROL, 0x0a, AM_ZP },
	{ &cpu::RLA, 0xff, AM_ZP },
	{ &cpu::PLP, 0x08, AM_NON },
	{ &cpu::AND, 0x04, AM_IMM },
	{ &cpu::ROLA, 0x04, AM_ACC },
	{ &cpu::AAC, 0xff, AM_IMM },
	{ &cpu::BIT, 0x08, AM_ABS },
	{ &cpu::AND, 0x08, AM_ABS },
	{ &cpu::ROL, 0x0c, AM_ABS },
	{ &cpu::RLA, 0xff, AM_ABS },
	{ &cpu::BMI, 0x05, AM_BRANCH },
	{ &cpu::AND, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::RLA, 0xff, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::AND, 0x08, AM_ZP_X },
	{ &cpu::ROL, 0x0c, AM_ZP_X },
	{ &cpu::RLA, 0xff, AM_ZP_X },
	{ &cpu::SEC, 0x04, AM_NON },
	{ &cpu::AND, 0x09, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::RLA, 0xff, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::AND, 0x09, AM_ABS_X },
	{ &cpu::ROL, 0x0e, AM_ABS_X },
	{ &cpu::RLA, 0xff, AM_ABS_X },
	{ &cpu::RTI, 0x0c, AM_NON },
	{ &cpu::EOR, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SRE, 0xff, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::EOR, 0x06, AM_ZP },
	{ &cpu::LSR, 0x0a, AM_ZP },
	{ &cpu::SRE, 0xff, AM_ZP },
	{ &cpu::PHA, 0x06, AM_NON },
	{ &cpu::EOR, 0x04, AM_IMM },
	{ &cpu::LSRA, 0x04, AM_ACC },
	{ &cpu::ALR, 0xff, AM_IMM },
	{ &cpu::JMP, 0x06, AM_ABS },
	{ &cpu::EOR, 0x08, AM_ABS },
	{ &cpu::LSR, 0x0c, AM_ABS },
	{ &cpu::SRE, 0xff, AM_ABS },
	{ &cpu::BVC, 0x05, AM_BRANCH },
	{ &cpu::EOR, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SRE, 0xff, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::EOR, 0x08, AM_ZP_X },
	{ &cpu::LSR, 0x0c, AM_ZP_X },
	{ &cpu::SRE, 0xff, AM_ZP_X },
	{ &cpu::CLI, 0x04, AM_NON },
	{ &cpu::EOR, 0x09, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SRE, 0xff, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::EOR, 0x09, AM_ABS_X },
	{ &cpu::LSR, 0x0e, AM_ABS_X },
	{ &cpu::SRE, 0xff, AM_ABS_X },
	{ &cpu::RTS, 0x0c, AM_NON },
	{ &cpu::ADC, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::RRA, 0xff, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ADC, 0x06, AM_ZP },
	{ &cpu::ROR, 0x0a, AM_ZP },
	{ &cpu::RRA, 0xff, AM_ZP },
	{ &cpu::PLA, 0x08, AM_NON },
	{ &cpu::ADC, 0x04, AM_IMM },
	{ &cpu::RORA, 0x04, AM_ACC },
	{ &cpu::ARR, 0xff, AM_IMM },
	{ &cpu::JMP, 0x0a, AM_REL },
	{ &cpu::ADC, 0x08, AM_ABS },
	{ &cpu::ROR, 0x0c, AM_ABS },
	{ &cpu::RRA, 0xff, AM_ABS },
	{ &cpu::BVS, 0x05, AM_BRANCH },
	{ &cpu::ADC, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::RRA, 0xff, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ADC, 0x08, AM_ZP_X },
	{ &cpu::ROR, 0x0c, AM_ZP_X },
	{ &cpu::RRA, 0xff, AM_ZP_X },
	{ &cpu::SEI, 0x04, AM_NON },
	{ &cpu::ADC, 0x09, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::RRA, 0xff, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ADC, 0x09, AM_ABS_X },
	{ &cpu::ROR, 0x0e, AM_ABS_X },
	{ &cpu::RRA, 0xff, AM_ABS_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::STA, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SAX, 0xff, AM_ZP_REL_Y },
	{ &cpu::STY, 0x06, AM_ZP },
	{ &cpu::STA, 0x06, AM_ZP },
	{ &cpu::STX, 0x06, AM_ZP },
	{ &cpu::SAX, 0xff, AM_ZP },
	{ &cpu::DEY, 0x04, AM_NON },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::TXA, 0x04, AM_NON },
	{ &cpu::XAA, 0xff, AM_IMM },
	{ &cpu::STY, 0x08, AM_ABS },
	{ &cpu::STA, 0x08, AM_ABS },
	{ &cpu::STX, 0x08, AM_ABS },
	{ &cpu::SAX, 0xff, AM_ABS },
	{ &cpu::BCC, 0x05, AM_BRANCH },
	{ &cpu::STA, 0x0c, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::AHX, 0xff, AM_ZP_REL_Y },
	{ &cpu::STY, 0x08, AM_ZP_X },
	{ &cpu::STA, 0x08, AM_ZP_X },
	{ &cpu::STX, 0x08, AM_ZP_Y },
	{ &cpu::SAX, 0xff, AM_ZP_Y },
	{ &cpu::TYA, 0x04, AM_NON },
	{ &cpu::STA, 0x0a, AM_ABS_Y },
	{ &cpu::TXS, 0x04, AM_NON },
	{ &cpu::TAS, 0xff, AM_ABS_Y },
	{ &cpu::SHY, 0xff, AM_ABS_X },
	{ &cpu::STA, 0x0a, AM_ABS_X },
	{ &cpu::SHX, 0xff, AM_ABS_Y },
	{ &cpu::AHX, 0xff, AM_ABS_Y },
	{ &cpu::LDY, 0x04, AM_IMM },
	{ &cpu::LDA, 0x0c, AM_ZP_REL_X },
	{ &cpu::LDX, 0x04, AM_IMM },
	{ &cpu::LAX, 0xff, AM_ZP_REL_Y },
	{ &cpu::LDY, 0x06, AM_ZP },
	{ &cpu::LDA, 0x06, AM_ZP },
	{ &cpu::LDX, 0x06, AM_ZP },
	{ &cpu::LAX, 0xff, AM_ZP },
	{ &cpu::TAY, 0x04, AM_NON },
	{ &cpu::LDA, 0x04, AM_IMM },
	{ &cpu::TAX, 0x04, AM_NON },
	{ &cpu::LAX, 0xff, AM_IMM },
	{ &cpu::LDY, 0x08, AM_ABS },
	{ &cpu::LDA, 0x08, AM_ABS },
	{ &cpu::LDX, 0x08, AM_ABS },
	{ &cpu::LAX, 0xff, AM_ABS },
	{ &cpu::BCS, 0x05, AM_BRANCH },
	{ &cpu::LDA, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::LDY, 0x08, AM_ZP_X },
	{ &cpu::LDA, 0x08, AM_ZP_X },
	{ &cpu::LDX, 0x08, AM_ZP_Y },
	{ &cpu::LAX, 0xff, AM_ZP_Y },
	{ &cpu::CLV, 0x04, AM_NON },
	{ &cpu::LDA, 0x09, AM_ABS_Y },
	{ &cpu::TSX, 0x04, AM_NON },
	{ &cpu::LAS, 0xff, AM_ABS_Y },
	{ &cpu::LDY, 0x09, AM_ABS_X },
	{ &cpu::LDA, 0x09, AM_ABS_X },
	{ &cpu::LDX, 0x09, AM_ABS_Y },
	{ &cpu::LAX, 0xff, AM_ABS_Y },
	{ &cpu::CPY, 0x04, AM_IMM },
	{ &cpu::CMP, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::DCP, 0xff, AM_ZP_REL_X },
	{ &cpu::CPY, 0x06, AM_ZP },
	{ &cpu::CMP, 0x06, AM_ZP },
	{ &cpu::DEC, 0x0a, AM_ZP },
	{ &cpu::DCP, 0xff, AM_ZP },
	{ &cpu::INY, 0x04, AM_NON },
	{ &cpu::CMP, 0x04, AM_IMM },
	{ &cpu::DEX, 0x04, AM_NON },
	{ &cpu::AXS, 0xff, AM_IMM },
	{ &cpu::CPY, 0x08, AM_ABS },
	{ &cpu::CMP, 0x08, AM_ABS },
	{ &cpu::DEC, 0x0c, AM_ABS },
	{ &cpu::DCP, 0xff, AM_ABS },
	{ &cpu::BNE, 0x05, AM_BRANCH },
	{ &cpu::CMP, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::DCP, 0xff, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::CMP, 0x08, AM_ZP_X },
	{ &cpu::DEC, 0x0c, AM_ZP_X },
	{ &cpu::DCP, 0xff, AM_ZP_X },
	{ &cpu::CLD, 0x04, AM_NON },
	{ &cpu::CMP, 0x09, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::DCP, 0xff, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::CMP, 0x09, AM_ABS_X },
	{ &cpu::DEC, 0x0e, AM_ABS_X },
	{ &cpu::DCP, 0xff, AM_ABS_X },
	{ &cpu::CPX, 0x04, AM_IMM },
	{ &cpu::SBC, 0x0c, AM_ZP_REL_X },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ISC, 0xff, AM_ZP_REL_X },
	{ &cpu::CPX, 0x06, AM_ZP },
	{ &cpu::SBC, 0x06, AM_ZP },
	{ &cpu::INC, 0x0a, AM_ZP },
	{ &cpu::ISC, 0xff, AM_ZP },
	{ &cpu::INX, 0x04, AM_NON },
	{ &cpu::SBC, 0x04, AM_IMM },
	{ &cpu::NOP, 0x04, AM_NON },
	{ &cpu::SBI, 0xff, AM_IMM },
	{ &cpu::CPX, 0x08, AM_ABS },
	{ &cpu::SBC, 0x08, AM_ABS },
	{ &cpu::INC, 0x0c, AM_ABS },
	{ &cpu::ISC, 0xff, AM_ABS },
	{ &cpu::BEQ, 0x05, AM_BRANCH },
	{ &cpu::SBC, 0x0b, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ISC, 0xff, AM_ZP_Y_REL },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SBC, 0x08, AM_ZP_X },
	{ &cpu::INC, 0x0c, AM_ZP_X },
	{ &cpu::ISC, 0xff, AM_ZP_X },
	{ &cpu::SED, 0x04, AM_NON },
	{ &cpu::SBC, 0x09, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::ISC, 0xff, AM_ABS_Y },
	{ &cpu::INV, 0xff, AM_NON },
	{ &cpu::SBC, 0x09, AM_ABS_X },
	{ &cpu::INC, 0x0e, AM_ABS_X },
	{ &cpu::ISC, 0xff, AM_ABS_X },
};

uint16_t cpu::GetArg(AddressModes mode) {
	uint16_t l, h;
	switch (mode) {
		case AM_ACC:
			return 0;

		case AM_IMM:
			return r.PC++;

		case AM_ABS:
			l = GetByte(r.PC++);
			h = GetByte(r.PC++);
			return l + (h << 8);

		case AM_ZP:
			return GetByte(r.PC++);

		case AM_NON:
			return 0;

		case AM_BRANCH:
			return r.PC + int8_t(GetByte(r.PC++));

		case AM_REL:
			l = GetByte(r.PC++);
			h = GetByte(r.PC++);
			return GetByte((h << 8) | l) + (GetByte((h << 8) | ((l+1)&0xff))<<8);

		case AM_ZP_X:
			return (GetByte(r.PC++) + r.X) & 0xff;

		case AM_ZP_Y:
			return (GetByte(r.PC++) + r.Y) & 0xff;

		case AM_ABS_X:
			l = GetByte(r.PC++);
			h = GetByte(r.PC++);
			penalty = (l+r.X)>=0x100;
			return l + (h << 8) + r.X;

		case AM_ABS_Y:
			l = GetByte(r.PC++);
			h = GetByte(r.PC++);
			penalty = (l+r.Y)>=0x100;
			return l + (h << 8) + r.Y;

		case AM_ZP_REL_X:
			l = (GetByte(r.PC++) + r.X) & 0xff;
			h = (l + 1) & 0xff;
			return GetByte(l) + (GetByte(h) << 8);

		case AM_ZP_Y_REL:
			l = GetByte(r.PC++);
			h = (l + 1) & 0xff;
			penalty = (l + r.Y) >= 0x100;
			return GetByte(l) + (GetByte(h) << 8) + r.Y;
	}
	return 0;
}

Regs cpu::Reset()
{
	r.A = 0;
	r.Y = 0;
	r.X = 0;
	r.PC = (GetByte(addr_reset_h) << 8) + GetByte(addr_reset_l);
	r.S = 0xFD;
	r.P |= F_U;
	r.T = 6;
	return r;
}

void cpu::Push(uint8_t byte)
{	
	SetByte(0x0100 + r.S, byte);
	r.S--;
}

uint8_t cpu::Pop()
{
	r.S++;
	return GetByte(0x0100 + r.S);
}

Regs cpu::IRQ()
{
	if(!ChkI()) {
		ClearFlag(F_B);
		Push((r.PC >> 8) & 0xff);
		Push(r.PC & 0xff);
		Push(r.P);
		SetFlag(F_I);
		r.PC = (GetByte(addr_irq_h) << 8) + GetByte(addr_irq_l);
	}
	return r;
}

Regs cpu::NMI()
{
	ClearFlag(F_B);
	Push((r.PC >> 8) & 0xff);
	Push(r.PC & 0xff);
	Push(r.P);
	SetFlag(F_I);
	r.PC = (GetByte(addr_nmi_h) << 8) + GetByte(addr_nmi_l);
	return r;
}

Regs cpu::Step()
{
	if (r.T != 0xff) {
		Opcode &instruction = table[GetByte(r.PC++)];
		(this->*instruction.code)(GetArg((AddressModes)instruction.mode));
		if (r.T != 0xff) {
			r.T = instruction.time == 0xff ?
				0xff : ((instruction.time>>1) + (penalty ? (instruction.time&1) : 0));
		}
	}
	return r;
}

void cpu::ADC(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	uint16_t tmp = m + r.A + (ChkC() ? 1 : 0);
	EvlZ(tmp);
	if (ChkD()) {
		if (((r.A & 0xF) + (m & 0xF) + (ChkC() ? 1 : 0)) > 9)
			tmp += 6;
		EvlN(tmp);
		int16_t vr = int16_t(int8_t(r.A)) + int16_t(int8_t(m+(r.P&1)));
		SetFlag(vr<-0x80 || vr>=0x80, F_V);
		if (tmp > 0x99)
			tmp += 96;
		SetC(tmp > 0x99);
	} else {
		EvlN(tmp);
		int16_t vr = int16_t(int8_t(r.A)) + int16_t(int8_t(m+(r.P&1)));
		SetFlag(vr<-0x80 || vr>=0x80, F_V);
		SetC(tmp > 0xff);
	}
	r.A = tmp & 0xff;
}

void cpu::AND(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	uint8_t res = m & r.A;
	EvlN(res);
	EvlZ(res);
	r.A = res;
}

void cpu::ASL(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	SetC(!!(m & 0x80));
	m <<= 1;
	m &= 0xff;
	EvlN(m);
	EvlZ(m);
	SetByte(arg, m);
}

void cpu::ASLA(uint16_t arg)
{
	uint8_t m = r.A;
	SetC(!!(m & 0x80));
	m <<= 1;
	EvlN(m);
	EvlZ(m);
	r.A = m;
}

void cpu::BCC(uint16_t arg)
{
	if (!ChkC()) {
		r.PC = arg;
		penalty = true;
	}
}


void cpu::BCS(uint16_t arg)
{
	if (ChkC()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::BEQ(uint16_t arg)
{
	if (ChkZ()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::BIT(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	uint8_t res = m & r.A;
	EvlN(res);
	r.P = (r.P & 0x3F) | (uint8_t)(m & 0xC0);
	EvlZ(res);
}

void cpu::BMI(uint16_t arg)
{
	if (ChkN()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::BNE(uint16_t arg)
{
	if (!ChkZ()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::BPL(uint16_t arg)
{
	if (!ChkN()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::BRK(uint16_t arg)
{
	r.PC++;
	Push((r.PC >> 8) & 0xff);
	Push(r.PC & 0xff);
	Push(r.P | F_B);
	SetFlag(F_I);
	r.PC = (GetByte(addr_irq_h) << 8) + GetByte(addr_irq_l);
}

void cpu::BVC(uint16_t arg)
{
	if (!ChkV()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::BVS(uint16_t arg)
{
	if (ChkV()) {
		r.PC = arg;
		penalty = true;
	}
}

void cpu::CLC(uint16_t arg)
{
	ClearFlag(F_C);
}

void cpu::CLD(uint16_t arg)
{
	ClearFlag(F_D);
}

void cpu::CLI(uint16_t arg)
{
	ClearFlag(F_I);
}

void cpu::CLV(uint16_t arg)
{
	ClearFlag(F_V);
}

void cpu::CMP(uint16_t arg)
{
	uint16_t tmp = r.A - GetByte(arg);
	SetC(tmp < 0x100);
	EvlN(tmp);
	EvlZ(tmp);
}

void cpu::CPX(uint16_t arg)
{
	uint16_t tmp = r.X - GetByte(arg);
	SetC(tmp < 0x100);
	EvlN(tmp);
	EvlZ(tmp);
}

void cpu::CPY(uint16_t arg)
{
	uint16_t tmp = r.Y - GetByte(arg);
	SetC(tmp < 0x100);
	EvlN(tmp);
	EvlZ(tmp);
}

void cpu::DEC(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	m = (m - 1) & 0xff;
	EvlN(m);
	EvlZ(m);
	SetByte(arg, m);
}

void cpu::DEX(uint16_t arg)
{
	uint8_t m = r.X;
	m = (m - 1) & 0xff;
	EvlN(m);
	EvlZ(m);
	r.X = m;
}

void cpu::DEY(uint16_t arg)
{
	uint8_t m = r.Y;
	m = (m - 1) & 0xff;
	EvlN(m);
	EvlZ(m);
	r.Y = m;
}

void cpu::EOR(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	m = r.A ^ m;
	EvlN(m);
	EvlZ(m);
	r.A = m;
}

void cpu::INC(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	m = (m + 1) & 0xff;
	EvlN(m);
	EvlZ(m);
	SetByte(arg, m);
}

void cpu::INX(uint16_t arg)
{
	uint8_t m = r.X;
	m = (m + 1) & 0xff;
	EvlN(m);
	EvlZ(m);
	r.X = m;
}

void cpu::INY(uint16_t arg)
{
	uint8_t m = r.Y;
	m = (m + 1) & 0xff;
	EvlN(m);
	EvlZ(m);
	r.Y = m;
}

void cpu::JMP(uint16_t arg)
{
	r.PC = arg;
}

void cpu::JSR(uint16_t arg)
{
	r.PC--;
	Push((r.PC >> 8) & 0xff);
	Push(r.PC & 0xff);
	r.PC = arg;
}

void cpu::LDA(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	EvlN(m);
	EvlZ(m);
	r.A = m;
}

void cpu::LDX(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	EvlN(m);
	EvlZ(m);
	r.X = m;
}

void cpu::LDY(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	EvlN(m);
	EvlZ(m);
	r.Y = m;
}

void cpu::LSR(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	SetC(m & 0x01);
	m >>= 1;
	ClearFlag(F_N);
	EvlZ(m);
	SetByte(arg, m);
}

void cpu::LSRA(uint16_t arg)
{
	uint8_t m = r.A;
	SetC(m & 0x01);
	m >>= 1;
	ClearFlag(F_N);
	EvlZ(m);
	r.A = m;
}

void cpu::NOP(uint16_t arg)
{
}

void cpu::ORA(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	m = r.A | m;
	EvlN(m);
	EvlZ(m);
	r.A = m;
}

void cpu::PHA(uint16_t arg)
{
	Push(r.A);
}

void cpu::PHP(uint16_t arg)
{	
	Push(r.P | F_B);
}

void cpu::PLA(uint16_t arg)
{
	r.A = Pop();
	EvlN(r.A);
	EvlZ(r.A);
}

void cpu::PLP(uint16_t arg)
{
	r.P = Pop();
	SetFlag(F_U);
}

void cpu::ROL(uint16_t arg)
{
	uint16_t m = GetByte(arg);
	m <<= 1;
	if (ChkC()) m |= 0x01;
	SetC(m > 0xff);
	m &= 0xff;
	EvlN(m);
	EvlZ(m);
	SetByte(arg, (uint8_t)m);
}

void cpu::ROLA(uint16_t arg)
{
	uint16_t m = r.A;
	m <<= 1;
	if (ChkC()) m |= 0x01;
	SetC(m > 0xff);
	m &= 0xff;
	EvlN(m);
	EvlZ(m);
	r.A = (uint8_t)m;
}

void cpu::ROR(uint16_t arg)
{
	uint16_t m = GetByte(arg);
	if (ChkC()) m |= 0x100;
	SetC(m & 0x01);
	m >>= 1;
	m &= 0xff;
	EvlN(m);
	EvlZ(m);
	SetByte(arg, (uint8_t)m);
}

void cpu::RORA(uint16_t arg)
{
	uint16_t m = r.A;
	if (ChkC()) m |= 0x100;
	SetC(m & 0x01);
	m >>= 1;
	m &= 0xff;
	EvlN(m);
	EvlZ(m);
	r.A = (uint8_t)m;
}

void cpu::RTI(uint16_t arg)
{
	uint8_t lo, hi;
	r.P = Pop();
	lo = Pop();
	hi = Pop();
	r.PC = (hi << 8) | lo;
}

void cpu::RTS(uint16_t arg)
{
	uint8_t lo, hi;
	lo = Pop();
	hi = Pop();
	r.PC = ((hi << 8) | lo) + 1;
}

void cpu::SBC(uint16_t arg)
{
	uint8_t m = GetByte(arg);
	uint16_t tmp = r.A - m - (ChkC() ? 0 : 1);
	EvlN(tmp);
	EvlZ(tmp);
	int16_t vr = int16_t(int8_t(r.A)) - int16_t(int8_t(m-((~r.P)&1)));
	SetFlag(vr<-0x80 || vr>=0x80, F_V);
	if (ChkD()) {
		if ( ((r.A & 0x0f) - (ChkC() ? 0 : 1)) < (arg & 0x0f)) tmp -= 6;
		if (tmp > 0x99)
			tmp -= 0x60;
	}
	SetC(tmp < 0x100);
	r.A = (tmp & 0xff);
}

void cpu::SEC(uint16_t arg)
{
	SetFlag(F_C);
}

void cpu::SED(uint16_t arg)
{
	SetFlag(F_D);
}

void cpu::SEI(uint16_t arg)
{
	SetFlag(F_I);
}

void cpu::STA(uint16_t arg)
{
	SetByte(arg, r.A);
}

void cpu::STX(uint16_t arg)
{
	SetByte(arg, r.X);
}

void cpu::STY(uint16_t arg)
{
	SetByte(arg, r.Y);
}

void cpu::TAX(uint16_t arg)
{
	uint8_t m = r.A;
	EvlN(m);
	EvlZ(m);
	r.X = m;
}

void cpu::TAY(uint16_t arg)
{
	uint8_t m = r.A;
	EvlN(m);
	EvlZ(m);
	r.Y = m;
}

void cpu::TSX(uint16_t arg)
{
	uint8_t m = r.S;
	EvlN(m);
	EvlZ(m);
	r.X = m;
}

void cpu::TXA(uint16_t arg)
{
	uint8_t m = r.X;
	EvlN(m);
	EvlZ(m);
	r.A = m;
}

void cpu::TXS(uint16_t arg)
{
	r.S = r.X;
}

void cpu::TYA(uint16_t arg)
{
	uint8_t m = r.Y;
	EvlN(m);
	EvlZ(m);
	r.A = m;
}


//
// External interface
//


Regs Step6502(const Regs &r, CBGetByte read, CBSetByte write)
{
	cpu mos(r, read, write);
	return mos.Step();
}

Regs IRQ6502(const Regs &r, CBGetByte read, CBSetByte write)
{
	cpu mos(r, read, write);
	return mos.IRQ();
}

Regs NMI6502(const Regs &r, CBGetByte read, CBSetByte write)
{
	cpu mos(r, read, write);
	return mos.NMI();
}

Regs Reset6502(const Regs &r, CBGetByte read, CBSetByte write)
{
	cpu mos(r, read, write);
	return mos.Reset();
}



