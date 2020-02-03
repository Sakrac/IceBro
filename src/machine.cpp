// 64k machine representation

#include "stdafx.h"
#include <stdio.h>
#include "machine.h"
#include "cpu.h"
#include "Sym.h"
#include "boot_ram.h"
#include "Expressions.h"
#include "struse/struse.h"
#include "platform.h"

#define UNDO_BUFFER_SIZE (16*1024*1024)
#define MAX_PC_BREAKPOINTS 256
#define CPU_EMULATOR_THREAD_STACK 8192
#define THREAD_CPU_CYCLES_PER_UPDATE 8000
#define MAX_BP_CONDITIONS 4*1024

uint8_t *ram = nullptr;
uint8_t *undo = nullptr;
uint32_t undo_oldest = UNDO_BUFFER_SIZE - 1;
uint32_t undo_newest = 0;
uint32_t history_max = 0;
uint32_t history_count = 0;

bool memChange = true;
bool memChangePrev = false;
bool sandboxContext = false;
Regs currRegs;
Regs prevRegs;
uint32_t cycles;
// if non-zero run this many instructions
uint32_t runCount = 0;


struct sBPCond {
	uint16_t offs;
	uint16_t size;
};

// in order to easily make a snapshot of breakpoints they are organized in
// a single array of PC breakpoints, then disabled breakpoints.
// This makes breakpoint management a little more tiptoey but
// checking for breakpoints is trivial and linear in memory which is more
// desired.

// breakpoints
static uint16_t aBP_PC[MAX_PC_BREAKPOINTS];	// active breakpoints
static uint32_t aBP_ID[MAX_PC_BREAKPOINTS];	// breakpoint IDs, for visualization
static sBPCond aBP_CN[MAX_PC_BREAKPOINTS];	// condition bytecode
static uint8_t aBP_EX[MAX_BP_CONDITIONS];
static uint16_t nBP = 0;					// total number of PC breakpoints
static uint16_t nBP_PC = 0;					// number of PC breakpoints
static uint16_t nBP_DS = 0;					// number of disabled breakpoints
static uint16_t nBP_EX_Len = 0;
static uint32_t nBP_NextID = 0;
static uint16_t runTo = 0xffff;
static uint16_t bStopCPU = 0;
static uint16_t bCPUIRQ = 0;
static uint16_t bCPUNMI = 0;

static IBMutex mutexBP = IBMutex_Clear;


// runs the CPU in a thread so the UI can continue
static IBThread hThreadCPU = IBThread_Clear;

static const char* aAddrModeFmt[] = {
	"%s ($%02x,x)",			// 00
	"%s $%02x",				// 01
	"%s #$%02x",			// 02
	"%s $%04x",				// 03
	"%s ($%02x),y",			// 04
	"%s $%02x,x",			// 05
	"%s $%04x,y",			// 06
	"%s $%04x,x",			// 07
	"%s ($%04x)",			// 08
	"%s A",					// 09
	"%s ",					// 0a
	"%s $%04x",				// 1a
	"%s ($%02x,y)",			// 17
	"%s $%02x,y",				// 16
};

static const char* aAddrModeLblFmt[] = {
	"%s (%s,x) ; $%02x",	// 00
	"%s %s ; $%02x",		// 01
	"%s #%s ; $%02x",		// 02
	"%s %s ; $%04x",		// 03
	"%s (%s),y ; $%02x",	// 04
	"%s %s,x ; $%02x",		// 05
	"%s %s,y ; $%04x",		// 06
	"%s %s,x ; $%04x",		// 07
	"%s (%s) ; $%04x",		// 08
	"%s A",					// 09
	"%s ",					// 0a
	"%s %s ; $%04x",		// 1a
	"%s (%s,y) ; %02x",		// 17
	"%s %s,y ; %02x" ,		// 16
};

const char *AddressModeNames[]{
	// address mode bit index

	// 6502

	"AM_ZP_REL_X",	// 0 ($12",x)
	"AM_ZP",			// 1 $12
	"AM_IMM",			// 2 #$12
	"AM_ABS",			// 3 $1234
	"AM_ZP_Y_REL",	// 4 ($12)",y
	"AM_ZP_X",		// 5 $12",x
	"AM_ABS_Y",		// 6 $1234",y
	"AM_ABS_X",		// 7 $1234",x
	"AM_REL",			// 8 ($1234)
	"AM_ACC",			// 9 A
	"AM_NON",			// a
	"AM_BRANCH",
	"AM_ZP_REL_Y",
	"AM_ZP_Y",
};

enum MNM_Base {
	mnm_brk,
	mnm_ora,
	mnm_cop,
	mnm_tsb,
	mnm_asl,
	mnm_php,
	mnm_phd,
	mnm_bpl,
	mnm_trb,
	mnm_clc,
	mnm_inc,
	mnm_tcs,
	mnm_jsr,
	mnm_and,
	mnm_bit,
	mnm_rol,
	mnm_plp,
	mnm_pld,
	mnm_bmi,
	mnm_sec,
	mnm_dec,
	mnm_tsc,
	mnm_rti,
	mnm_eor,
	mnm_wdm,
	mnm_mvp,
	mnm_lsr,
	mnm_pha,
	mnm_phk,
	mnm_jmp,
	mnm_bvc,
	mnm_mvn,
	mnm_cli,
	mnm_phy,
	mnm_tcd,
	mnm_rts,
	mnm_adc,
	mnm_per,
	mnm_stz,
	mnm_ror,
	mnm_rtl,
	mnm_bvs,
	mnm_sei,
	mnm_ply,
	mnm_tdc,
	mnm_bra,
	mnm_sta,
	mnm_brl,
	mnm_sty,
	mnm_stx,
	mnm_dey,
	mnm_txa,
	mnm_phb,
	mnm_bcc,
	mnm_tya,
	mnm_txs,
	mnm_txy,
	mnm_ldy,
	mnm_lda,
	mnm_ldx,
	mnm_tay,
	mnm_tax,
	mnm_plb,
	mnm_bcs,
	mnm_clv,
	mnm_tsx,
	mnm_tyx,
	mnm_cpy,
	mnm_cmp,
	mnm_rep,
	mnm_iny,
	mnm_dex,
	mnm_wai,
	mnm_bne,
	mnm_pei,
	mnm_cld,
	mnm_phx,
	mnm_stp,
	mnm_cpx,
	mnm_sbc,
	mnm_sep,
	mnm_inx,
	mnm_nop,
	mnm_xba,
	mnm_beq,
	mnm_pea,
	mnm_sed,
	mnm_plx,
	mnm_xce,
	mnm_inv,
	mnm_pla,

	mnm_wdc_and_illegal_instructions,

	mnm_bbs0 = mnm_wdc_and_illegal_instructions,
	mnm_bbs1,
	mnm_bbs2,
	mnm_bbs3,
	mnm_bbs4,
	mnm_bbs5,
	mnm_bbs6,
	mnm_bbs7,
	mnm_bbr0,
	mnm_bbr1,
	mnm_bbr2,
	mnm_bbr3,
	mnm_bbr4,
	mnm_bbr5,
	mnm_bbr6,
	mnm_bbr7,

	mnm_ahx,
	mnm_anc,
	mnm_aac,
	mnm_alr,
	mnm_axs,
	mnm_dcp,
	mnm_isc,
	mnm_lax,
	mnm_lax2,
	mnm_rla,
	mnm_rra,
	mnm_sre,
	mnm_sax,
	mnm_slo,
	mnm_xaa,
	mnm_arr,
	mnm_tas,
	mnm_shy,
	mnm_shx,
	mnm_las,
	mnm_sbi,

	mnm_count
};

const char *zsMNM[mnm_count]{
	"brk",
	"ora",
	"cop",
	"tsb",
	"asl",
	"php",
	"phd",
	"bpl",
	"trb",
	"clc",
	"inc",
	"tcs",
	"jsr",
	"and",
	"bit",
	"rol",
	"plp",
	"pld",
	"bmi",
	"sec",
	"dec",
	"tsc",
	"rti",
	"eor",
	"wdm",
	"mvp",
	"lsr",
	"pha",
	"phk",
	"jmp",
	"bvc",
	"mvn",
	"cli",
	"phy",
	"tcd",
	"rts",
	"adc",
	"per",
	"stz",
	"ror",
	"rtl",
	"bvs",
	"sei",
	"ply",
	"tdc",
	"bra",
	"sta",
	"brl",
	"sty",
	"stx",
	"dey",
	"txa",
	"phb",
	"bcc",
	"tya",
	"txs",
	"txy",
	"ldy",
	"lda",
	"ldx",
	"tay",
	"tax",
	"plb",
	"bcs",
	"clv",
	"tsx",
	"tyx",
	"cpy",
	"cmp",
	"rep",
	"iny",
	"dex",
	"wai",
	"bne",
	"pei",
	"cld",
	"phx",
	"stp",
	"cpx",
	"sbc",
	"sep",
	"inx",
	"nop",
	"xba",
	"beq",
	"pea",
	"sed",
	"plx",
	"xce",
	"???",
	"pla",
	"bbs0",
	"bbs1",
	"bbs2",
	"bbs3",
	"bbs4",
	"bbs5",
	"bbs6",
	"bbs7",
	"bbr0",
	"bbr1",
	"bbr2",
	"bbr3",
	"bbr4",
	"bbr5",
	"bbr6",
	"bbr7",
	"ahx",
	"anc",
	"aac",
	"alr",
	"axs",
	"dcp",
	"isc",
	"lax",
	"lax2",
	"rla",
	"rra",
	"sre",
	"sax",
	"slo",
	"xaa",
	"arr",
	"tas",
	"shy",
	"shx",
	"las",
	"sbi",
};

struct dismnm {
	MNM_Base mnemonic;
	unsigned char addrMode;
	unsigned char arg_size;
};

struct dismnm a6502_ops[256] = {
	{ mnm_brk, AM_NON, 0 },
	{ mnm_ora, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_slo, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ZP, 1 },
	{ mnm_asl, AM_ZP, 1 },
	{ mnm_slo, AM_ZP, 1 },
	{ mnm_php, AM_NON, 0 },
	{ mnm_ora, AM_IMM, 1 },
	{ mnm_asl, AM_NON, 0 },
	{ mnm_anc, AM_IMM, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ABS, 2 },
	{ mnm_asl, AM_ABS, 2 },
	{ mnm_slo, AM_ABS, 2 },
	{ mnm_bpl, AM_BRANCH, 1 },
	{ mnm_ora, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_slo, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ZP_X, 1 },
	{ mnm_asl, AM_ZP_X, 1 },
	{ mnm_slo, AM_ZP_X, 1 },
	{ mnm_clc, AM_NON, 0 },
	{ mnm_ora, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_slo, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ABS_X, 2 },
	{ mnm_asl, AM_ABS_X, 2 },
	{ mnm_slo, AM_ABS_X, 2 },
	{ mnm_jsr, AM_ABS, 2 },
	{ mnm_and, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rla, AM_ZP_REL_X, 1 },
	{ mnm_bit, AM_ZP, 1 },
	{ mnm_and, AM_ZP, 1 },
	{ mnm_rol, AM_ZP, 1 },
	{ mnm_rla, AM_ZP, 1 },
	{ mnm_plp, AM_NON, 0 },
	{ mnm_and, AM_IMM, 1 },
	{ mnm_rol, AM_NON, 0 },
	{ mnm_aac, AM_IMM, 1 },
	{ mnm_bit, AM_ABS, 2 },
	{ mnm_and, AM_ABS, 2 },
	{ mnm_rol, AM_ABS, 2 },
	{ mnm_rla, AM_ABS, 2 },
	{ mnm_bmi, AM_BRANCH, 1 },
	{ mnm_and, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rla, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_and, AM_ZP_X, 1 },
	{ mnm_rol, AM_ZP_X, 1 },
	{ mnm_rla, AM_ZP_X, 1 },
	{ mnm_sec, AM_NON, 0 },
	{ mnm_and, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rla, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_and, AM_ABS_X, 2 },
	{ mnm_rol, AM_ABS_X, 2 },
	{ mnm_rla, AM_ABS_X, 2 },
	{ mnm_rti, AM_NON, 0 },
	{ mnm_eor, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sre, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ZP, 1 },
	{ mnm_lsr, AM_ZP, 1 },
	{ mnm_sre, AM_ZP, 1 },
	{ mnm_pha, AM_NON, 0 },
	{ mnm_eor, AM_IMM, 1 },
	{ mnm_lsr, AM_NON, 0 },
	{ mnm_alr, AM_IMM, 1 },
	{ mnm_jmp, AM_ABS, 2 },
	{ mnm_eor, AM_ABS, 2 },
	{ mnm_lsr, AM_ABS, 2 },
	{ mnm_sre, AM_ABS, 2 },
	{ mnm_bvc, AM_BRANCH, 1 },
	{ mnm_eor, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sre, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ZP_X, 1 },
	{ mnm_lsr, AM_ZP_X, 1 },
	{ mnm_sre, AM_ZP_X, 1 },
	{ mnm_cli, AM_NON, 0 },
	{ mnm_eor, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sre, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ABS_X, 2 },
	{ mnm_lsr, AM_ABS_X, 2 },
	{ mnm_sre, AM_ABS_X, 2 },
	{ mnm_rts, AM_NON, 0 },
	{ mnm_adc, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rra, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_adc, AM_ZP, 1 },
	{ mnm_ror, AM_ZP, 1 },
	{ mnm_rra, AM_ZP, 1 },
	{ mnm_pla, AM_NON, 0 },
	{ mnm_adc, AM_IMM, 1 },
	{ mnm_ror, AM_NON, 0 },
	{ mnm_arr, AM_IMM, 1 },
	{ mnm_jmp, AM_REL, 2 },
	{ mnm_adc, AM_ABS, 2 },
	{ mnm_ror, AM_ABS, 2 },
	{ mnm_rra, AM_ABS, 2 },
	{ mnm_bvs, AM_BRANCH, 1 },
	{ mnm_adc, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rra, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_adc, AM_ZP_X, 1 },
	{ mnm_ror, AM_ZP_X, 1 },
	{ mnm_rra, AM_ZP_X, 1 },
	{ mnm_sei, AM_NON, 0 },
	{ mnm_adc, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rra, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_adc, AM_ABS_X, 2 },
	{ mnm_ror, AM_ABS_X, 2 },
	{ mnm_rra, AM_ABS_X, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sta, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sax, AM_ZP_REL_Y, 1 },
	{ mnm_sty, AM_ZP, 1 },
	{ mnm_sta, AM_ZP, 1 },
	{ mnm_stx, AM_ZP, 1 },
	{ mnm_sax, AM_ZP, 1 },
	{ mnm_dey, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_txa, AM_NON, 0 },
	{ mnm_xaa, AM_IMM, 1 },
	{ mnm_sty, AM_ABS, 2 },
	{ mnm_sta, AM_ABS, 2 },
	{ mnm_stx, AM_ABS, 2 },
	{ mnm_sax, AM_ABS, 2 },
	{ mnm_bcc, AM_BRANCH, 1 },
	{ mnm_sta, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ahx, AM_ZP_REL_Y, 1 },
	{ mnm_sty, AM_ZP_X, 1 },
	{ mnm_sta, AM_ZP_X, 1 },
	{ mnm_stx, AM_ZP_Y, 1 },
	{ mnm_sax, AM_ZP_Y, 1 },
	{ mnm_tya, AM_NON, 0 },
	{ mnm_sta, AM_ABS_Y, 2 },
	{ mnm_txs, AM_NON, 0 },
	{ mnm_tas, AM_ABS_Y, 2 },
	{ mnm_shy, AM_ABS_X, 2 },
	{ mnm_sta, AM_ABS_X, 2 },
	{ mnm_shx, AM_ABS_Y, 2 },
	{ mnm_ahx, AM_ABS_Y, 2 },
	{ mnm_ldy, AM_IMM, 1 },
	{ mnm_lda, AM_ZP_REL_X, 1 },
	{ mnm_ldx, AM_IMM, 1 },
	{ mnm_lax, AM_ZP_REL_Y, 1 },
	{ mnm_ldy, AM_ZP, 1 },
	{ mnm_lda, AM_ZP, 1 },
	{ mnm_ldx, AM_ZP, 1 },
	{ mnm_lax, AM_ZP, 1 },
	{ mnm_tay, AM_NON, 0 },
	{ mnm_lda, AM_IMM, 1 },
	{ mnm_tax, AM_NON, 0 },
	{ mnm_lax2, AM_IMM, 1 },
	{ mnm_ldy, AM_ABS, 2 },
	{ mnm_lda, AM_ABS, 2 },
	{ mnm_ldx, AM_ABS, 2 },
	{ mnm_lax, AM_ABS, 2 },
	{ mnm_bcs, AM_BRANCH, 1 },
	{ mnm_lda, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ldy, AM_ZP_X, 1 },
	{ mnm_lda, AM_ZP_X, 1 },
	{ mnm_ldx, AM_ZP_Y, 1 },
	{ mnm_lax, AM_ZP_Y, 1 },
	{ mnm_clv, AM_NON, 0 },
	{ mnm_lda, AM_ABS_Y, 2 },
	{ mnm_tsx, AM_NON, 0 },
	{ mnm_las, AM_ABS_Y, 2 },
	{ mnm_ldy, AM_ABS_X, 2 },
	{ mnm_lda, AM_ABS_X, 2 },
	{ mnm_ldx, AM_ABS_Y, 2 },
	{ mnm_lax, AM_ABS_Y, 2 },
	{ mnm_cpy, AM_IMM, 1 },
	{ mnm_cmp, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dcp, AM_ZP_REL_X, 1 },
	{ mnm_cpy, AM_ZP, 1 },
	{ mnm_cmp, AM_ZP, 1 },
	{ mnm_dec, AM_ZP, 1 },
	{ mnm_dcp, AM_ZP, 1 },
	{ mnm_iny, AM_NON, 0 },
	{ mnm_cmp, AM_IMM, 1 },
	{ mnm_dex, AM_NON, 0 },
	{ mnm_axs, AM_IMM, 1 },
	{ mnm_cpy, AM_ABS, 2 },
	{ mnm_cmp, AM_ABS, 2 },
	{ mnm_dec, AM_ABS, 2 },
	{ mnm_dcp, AM_ABS, 2 },
	{ mnm_bne, AM_BRANCH, 1 },
	{ mnm_cmp, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dcp, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cmp, AM_ZP_X, 1 },
	{ mnm_dec, AM_ZP_X, 1 },
	{ mnm_dcp, AM_ZP_X, 1 },
	{ mnm_cld, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dcp, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_X, 2 },
	{ mnm_dec, AM_ABS_X, 2 },
	{ mnm_dcp, AM_ABS_X, 2 },
	{ mnm_cpx, AM_IMM, 1 },
	{ mnm_sbc, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_isc, AM_ZP_REL_X, 1 },
	{ mnm_cpx, AM_ZP, 1 },
	{ mnm_sbc, AM_ZP, 1 },
	{ mnm_inc, AM_ZP, 1 },
	{ mnm_isc, AM_ZP, 1 },
	{ mnm_inx, AM_NON, 0 },
	{ mnm_sbc, AM_IMM, 1 },
	{ mnm_nop, AM_NON, 0 },
	{ mnm_sbi, AM_IMM, 1 },
	{ mnm_cpx, AM_ABS, 2 },
	{ mnm_sbc, AM_ABS, 2 },
	{ mnm_inc, AM_ABS, 2 },
	{ mnm_isc, AM_ABS, 2 },
	{ mnm_beq, AM_BRANCH, 1 },
	{ mnm_sbc, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_isc, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sbc, AM_ZP_X, 1 },
	{ mnm_inc, AM_ZP_X, 1 },
	{ mnm_isc, AM_ZP_X, 1 },
	{ mnm_sed, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_isc, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_X, 2 },
	{ mnm_inc, AM_ABS_X, 2 },
	{ mnm_isc, AM_ABS_X, 2 },
};

uint8_t Get6502Byte(uint16_t addr);
void Set6502Byte(uint16_t addr, uint8_t value);
void Set6502ByteRecord(uint16_t addr, uint8_t value);


void Initialize6502()
{
	ram = (uint8_t*)calloc(64, 1024);
	//int GetBlocks6502(struct block6502 **ppBlocks);
	struct block6502* pBlx;
	int memBlocks = GetBlocks6502(&pBlx);
	for (int b = 0; b < memBlocks; b++)
		memcpy(ram + pBlx[b].address, pBlx[b].data, pBlx[b].size);
	memset(ram + 0xd800, 0xfe, 1000);

	memChange = true;

	undo = (uint8_t*)malloc(UNDO_BUFFER_SIZE);
	undo_oldest = UNDO_BUFFER_SIZE - 1;
	undo_newest = 0;
	history_max = 0;
	history_count = 0;
	if (undo) {
		undo[0] = 0;
		undo[UNDO_BUFFER_SIZE - 1] = 0;
	}
	IBMutexInit(&mutexBP, "6502 Context");

	currRegs = Reset6502(currRegs, Get6502Byte, Set6502Byte);

	sandboxContext = true;
}

void Shutdown6502()
{
	if (hThreadCPU) {
		CPUStop();
		while (hThreadCPU)
			Sleep(1);
	}

	IBMutexDestroy(&mutexBP);

	free(ram);
	free(undo);
}

void ResetUndoBuffer()
{
	undo_oldest = UNDO_BUFFER_SIZE - 1;
	undo_newest = 0;
	history_max = 0;
	history_count = 0;
	undo[0] = 0;
}

void CheckRegChange()
{
	bool curr = memChange;
	if (prevRegs != currRegs) {
		prevRegs = currRegs;
		memChange = true;
	}
	if (memChangePrev) { memChange = true; }
	memChangePrev = curr;
}

Regs& GetRegs()
{
	return currRegs;
}

void SetRegs(const Regs &r)
{
	currRegs = r;
}

uint32_t GetCycles()
{
	return cycles;
}

uint8_t *Get6502Mem(uint16_t addr)
{
	return ram + addr;
}

bool IsSandboxContext() { return sandboxContext; }

void SetSandboxContext(bool set) { sandboxContext = set; }

uint8_t Get6502Byte(uint16_t addr)
{
	return ram[addr];
}

void Set6502Byte(uint16_t addr, uint8_t value)
{
	if (ram[addr] != value) { memChange = true; }
	ram[addr] = value;
}

void PushUndoByte(uint8_t b)
{
	undo[undo_newest] = b;
	uint32_t p = undo_newest;
	undo_newest++;
	if (undo_newest == UNDO_BUFFER_SIZE)
		undo_newest = 0;
	if (undo_oldest == p)
		undo_oldest = undo_newest;
}

bool HaveUndoStep()
{
	if (undo[undo_newest]) {
		uint32_t size = sizeof(Regs) + 3 * (undo[undo_newest] - 1);
		uint32_t buf = (undo_newest - undo_oldest) % UNDO_BUFFER_SIZE;
		return !buf || size <= buf;
	}
	return false;
}

uint8_t PopUndoByte()
{
	if (undo_newest)
		undo_newest--;
	else
		undo_newest = UNDO_BUFFER_SIZE - 1;
	return undo[undo_newest];
}

void Set6502ByteRecord(uint16_t addr, uint8_t value)
{
	if (ram[addr] != value) {
		uint8_t changes = undo[undo_newest];
		PushUndoByte(ram[addr]);
		PushUndoByte((uint8_t)(addr >> 8));
		PushUndoByte((uint8_t)addr);
		undo[undo_newest] = changes + 1;
		ram[addr] = value;
		memChange = true;
	}
}

void CPUAddUndoRegs(Regs &regs)
{
	// undo[undo_newest] contains the byte size of the state change for the previous byte
	undo_newest = (undo_newest + 1) % UNDO_BUFFER_SIZE;
	for (size_t c = 0; c < sizeof(Regs); c++)
		PushUndoByte(((uint8_t*)&regs)[c]);
	undo[undo_newest] = 1;	// stored regs
}

void CPUStepInt()
{
	CPUAddUndoRegs(currRegs);
	currRegs = Step6502(currRegs, Get6502Byte, Set6502ByteRecord);
	if (currRegs.T != 0xff)
		cycles += currRegs.T;
	++history_count;
	if (history_count > history_max) { history_max = history_count; }
	if (runCount) { --runCount; }
}

void CPUGoThread();
void CPUReverseThread();

bool CheckPCBreakpoint(uint16_t addr, const uint16_t num = nBP, const uint16_t *cmp = aBP_PC,
					   const struct sBPCond *cond = aBP_CN, const uint8_t *expr = aBP_EX)
{
	for (uint16_t b = 0; b < num; b++) {
		if (cmp[b] == addr)
			return !cond[b].size || EvalExpression(expr + cond[b].offs);
	}
	return false;
}

void CPUStepOver()
{
	// can not step while CPU is running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	if (Get6502Byte(currRegs.PC) == 0x20) {
		uint16_t ret = currRegs.PC + 3;
		uint32_t c = cycles;
		do {
			CPUStepInt();
		} while (currRegs.PC != ret && (cycles - c) < 64 && !CheckPCBreakpoint(currRegs.PC));
		if (currRegs.PC != ret) {
			runTo = ret;
			CPUGoThread();
		}
	} else
		CPUStepInt();

}

bool CPUStepBackInt(Regs &regs, uint32_t &stepCycles)
{
	if (HaveUndoStep()) {
		if (uint32_t stored = undo[undo_newest]) { // 0 means at end of buffer
			if (regs.T != 0xff)
				stepCycles -= regs.T;

			if (history_count) { --history_count; }
			if (runCount) { --runCount; }

			uint8_t change[3];
			for (uint32_t c = 1; c < stored; c++) {
				for (int i = 0; i < 3; i++)
					change[i] = PopUndoByte();
				uint16_t addr = (uint16_t(change[1]) << 8) + change[0];
				if (ram[addr] != change[2]) { memChange = true; }
				ram[addr] = change[2];
			}

			for (size_t c = 0; c < sizeof(Regs); c++)
				((uint8_t*)&regs)[sizeof(Regs) - 1 - c] = PopUndoByte();
			PopUndoByte();
			return true;
		}
	}
	history_count = 0;
	return false;
}

bool CPUStepBack()
{
	sandboxContext = true;
	bool ret = CPUStepBackInt(currRegs, cycles);
	return ret;
}

void CPUStepOverBack()
{
	// can not step while CPU is running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	if (Get6502Byte(currRegs.PC - 3) == 0x20) {
		uint16_t ret = currRegs.PC - 3;
		uint32_t c = cycles;
		do {
			CPUStepBackInt(currRegs, cycles);
		} while (currRegs.PC != ret && (cycles - c) < 64 && !CheckPCBreakpoint(currRegs.PC) && HaveUndoStep());
		if (currRegs.PC != ret && HaveUndoStep()) {
			runTo = ret;
			CPUReverseThread();
		}
	} else
		CPUStepBackInt(currRegs, cycles);

	memChange = true;
}

bool IsCPURunning()
{
	return hThreadCPU != IBThread_Clear;
}

bool MemoryChange()
{
	return memChange || memChangePrev;
}

void ClearMemoryChange()
{
	memChange = false;
}


void CPUStop()
{
	while (1 != InterlockedExchange16((SHORT*)&bStopCPU, 1)) {}
}

void CPUStep()
{
	// can not step while CPU is running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	CPUStepInt();
}

/*
struct sBPCond _aBP_CN[MAX_PC_BREAKPOINTS];
uint8_t _aBP_EX[MAX_BP_CONDITIONS];
*/

void CPUGo(uint32_t numInstructions)
{
	// can not step while CPU is running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	runCount = numInstructions;
	uint32_t c = cycles;
	do {
		CPUStep();
		if (numInstructions && !runCount) { break; }
		if (currRegs.T == 0xff)
			break;
		if ((cycles - c) > 64) {
			CPUGoThread();
			return;
		}
	} while (numInstructions || !CheckPCBreakpoint(currRegs.PC));
}

void CPURunTo(uint16_t stopAddr)
{
	if (IsCPURunning())
		return;

	sandboxContext = true;
	uint32_t c = cycles;
	do {
		if (currRegs.PC == stopAddr) { break; }
		CPUStep();
		if (currRegs.T == 0xff)
			break;
		if ((cycles - c) > 64) {
			runTo = stopAddr;
			CPUGoThread();
			return;
		}
	} while (!CheckPCBreakpoint(currRegs.PC));
}


void CPUReverse(uint32_t numInstructions)
{
	// can not step while CPU is running
	if (IsCPURunning())
		return;

	runCount = numInstructions;
	sandboxContext = true;
	uint32_t c = cycles;
	do {
		if (!CPUStepBackInt(currRegs, cycles))
			break;
		if (numInstructions && !runCount) { break; }
		if ((c - cycles) > 64) {
			CPUReverseThread();
			return;
		}
	} while (numInstructions || CheckPCBreakpoint(currRegs.PC));
}

void CPUReverseTo(uint16_t stopAddr)
{
	// can not step while CPU is running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	uint32_t c = cycles;
	do {
		if (!CPUStepBackInt(currRegs, cycles))
			break;
		if ((c - cycles) > 64) {
			runTo = stopAddr;
			CPUReverseThread();
			return;
		}
	} while (!CheckPCBreakpoint(currRegs.PC) && currRegs.PC != stopAddr);
}

static IBThreadRet CPUGoThreadRun(void *param)
{
	// copy breakpoints to stack so UI can modify original freely
	uint16_t _aBP_PC[MAX_PC_BREAKPOINTS];
	struct sBPCond _aBP_CN[MAX_PC_BREAKPOINTS];
	uint8_t _aBP_EX[MAX_BP_CONDITIONS];
	uint16_t _nBP_PC = nBP;
	uint16_t _runTo = runTo;
	uint32_t _runCount = runCount;
	uint32_t _history_count = history_count;
	runTo = 0xffff;
	runCount = 0;

	IBMutexLock(&mutexBP);

	memcpy(_aBP_PC, aBP_PC, sizeof(uint16_t) * _nBP_PC);
	memcpy(_aBP_CN, aBP_CN, sizeof(_aBP_CN[0]) * _nBP_PC);
	if (nBP_EX_Len)
		memcpy(_aBP_EX, aBP_EX, nBP_EX_Len);

	// keep regs and cycles on stack for the same reason
	Regs stackRegs = currRegs;
	uint32_t stackCycles = cycles;
	uint32_t updateCycles = cycles;
	bStopCPU = 0;
	bCPUIRQ = 0;
	bCPUIRQ = 0;

	IBMutexRelease(&mutexBP);

	do {
		CPUAddUndoRegs(stackRegs);
		stackRegs = Step6502(stackRegs, Get6502Byte, Set6502ByteRecord);
		if (stackRegs.T != 0xff)
			stackCycles += stackRegs.T;
		else
			break;
		++_history_count;

		if (bCPUIRQ) {
			CPUAddUndoRegs(stackRegs);
			stackRegs = IRQ6502(stackRegs, Get6502Byte, Set6502ByteRecord);
			while (0 != InterlockedExchange16((SHORT*)&bCPUIRQ, 0)) {}
		}

		if (bCPUNMI) {
			CPUAddUndoRegs(stackRegs);
			stackRegs = NMI6502(stackRegs, Get6502Byte, Set6502ByteRecord);
			while (0 != InterlockedExchange16((SHORT*)&bCPUNMI, 0)) {}
		}

		if (_runTo != 0xffff && stackRegs.PC == _runTo) { break; }
		if (_runCount) {
			_runCount--;
			if (!_runCount) { break; }
		}

		if ((stackCycles - updateCycles) > THREAD_CPU_CYCLES_PER_UPDATE) {
			Sleep(1);
			updateCycles = stackCycles;
			IBMutexLock(&mutexBP);
			_nBP_PC = nBP;
			memcpy(_aBP_PC, aBP_PC, sizeof(uint16_t) * _nBP_PC);
			memcpy(_aBP_CN, aBP_CN, sizeof(_aBP_CN[0]) * _nBP_PC);
			if (nBP_EX_Len)
				memcpy(_aBP_EX, aBP_EX, nBP_EX_Len);
			currRegs = stackRegs;
			cycles = stackCycles;
			uint16_t stopped = bStopCPU;
			IBMutexRelease(&mutexBP);
			if (stopped)
				break;
		}
	} while (!CheckPCBreakpoint(stackRegs.PC, _nBP_PC, _aBP_PC, _aBP_CN, aBP_EX));

	currRegs = stackRegs;
	cycles = stackCycles;
	history_count = _history_count;
	if (_history_count > history_max) { history_max = _history_count; }

	// the CPU thread is finished
	hThreadCPU = IBThread_Clear;
	return NULL;
}

void CPUGoThread()
{
	// can't start running if already running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	IBCreateThread(&hThreadCPU, CPU_EMULATOR_THREAD_STACK, CPUGoThreadRun, nullptr);
}


static IBThreadRet CPUReverseThreadRun(void *param)
{
	// copy breakpoints to stack so UI can modify original freely
	uint16_t _aBP_PC[MAX_PC_BREAKPOINTS];
	struct sBPCond _aBP_CN[MAX_PC_BREAKPOINTS];
	uint8_t _aBP_EX[MAX_BP_CONDITIONS];
	uint16_t _nBP_PC = nBP;
	uint16_t _runTo = runTo;
	uint32_t _runCount = runCount;
	uint32_t _history_count = history_count;
	runTo = 0xffff;
	runCount = 0;

	IBMutexLock(&mutexBP);

	memcpy(_aBP_PC, aBP_PC, sizeof(uint16_t) * _nBP_PC);
	memcpy(_aBP_CN, aBP_CN, sizeof(_aBP_CN[0]) * _nBP_PC);
	if (nBP_EX_Len)
		memcpy(_aBP_EX, aBP_EX, nBP_EX_Len);

	// keep regs and cycles on stack for the same reason
	Regs stackRegs = currRegs;
	uint32_t stackCycles = cycles;
	uint32_t updateCycles = cycles;
	bStopCPU = 0;
	bCPUIRQ = 0;
	bCPUIRQ = 0;

	IBMutexRelease(&mutexBP);

	do {
		if (_runTo != 0xffff && stackRegs.PC == _runTo) { break; }

		bool hadStep = CPUStepBackInt(stackRegs, stackCycles);
		if (_history_count) { --_history_count; }

		if (_runCount) {
			_runCount--;
			if (!_runCount) { break; }
		}

		if (!hadStep || (updateCycles - stackCycles) > THREAD_CPU_CYCLES_PER_UPDATE) {
			Sleep(1);
			updateCycles = stackCycles;
			IBMutexLock(&mutexBP);
			_nBP_PC = nBP;
			memcpy(_aBP_PC, aBP_PC, sizeof(uint16_t) * _nBP_PC);
			currRegs = stackRegs;
			cycles = stackCycles;
			uint16_t stopped = bStopCPU;
			IBMutexRelease(&mutexBP);
			if (stopped || !hadStep)
				break;
		}
	} while (!CheckPCBreakpoint(stackRegs.PC, _nBP_PC, _aBP_PC, _aBP_CN, aBP_EX));

	currRegs = stackRegs;
	cycles = stackCycles;
	history_count = _history_count;
	runCount = _runCount;

	// the CPU thread is finished
	hThreadCPU = IBThread_Clear;
	return 0;
}

void CPUReverseThread()
{
	// can't start running if already running
	if (IsCPURunning())
		return;

	sandboxContext = true;
	IBCreateThread(&hThreadCPU, CPU_EMULATOR_THREAD_STACK, CPUReverseThreadRun, nullptr);
}



void CPUReset()
{
	if (!IsCPURunning()) {
		sandboxContext = true;
		CPUAddUndoRegs(currRegs);
		currRegs = Reset6502(currRegs, Get6502Byte, Set6502ByteRecord);
	}
}

void CPUIRQ()
{
	if (IsCPURunning()) {
		while (1 != InterlockedExchange16((SHORT*)&bCPUIRQ, 1)) {}
	} else {
		CPUAddUndoRegs(currRegs);
		currRegs = IRQ6502(currRegs, Get6502Byte, Set6502ByteRecord);
	}
}

void CPUNMI()
{
	if (IsCPURunning()) {
		while (1 != InterlockedExchange16((SHORT*)&bCPUNMI, 1)) {}
	} else {
		CPUAddUndoRegs(currRegs);
		currRegs = NMI6502(currRegs, Get6502Byte, Set6502ByteRecord);
	}
}

uint32_t GetHistoryCount(uint32_t &maxCount)
{
	maxCount = history_max;
	return history_count;
}

uint16_t GetPCBreakpointsID(uint16_t **pBP, uint32_t **pID, uint16_t &nDS)
{
	*pBP = aBP_PC;
	*pID = aBP_ID;
	nDS = nBP_DS;
	return nBP;
}

uint16_t GetNumPCBreakpoints()
{
	return nBP;
}

uint16_t* GetPCBreakpoints()
{
	return aBP_PC;
}

void EraseBPCondition(uint16_t index)
{
	if (aBP_CN[index].size != 0) {
		uint16_t o = aBP_CN[index].offs;
		uint16_t s = aBP_CN[index].size;
		uint8_t *w = aBP_EX + o;
		const uint8_t *r = w + s;
		uint16_t m = nBP_EX_Len - o - s;
		for (int b = 0; b < m; b++)
			*w++ = *r++;
		aBP_CN[index].size = 0;
		for (uint16_t i = 0; i < nBP; i++) {
			if (aBP_CN[i].size && aBP_CN[i].offs > o)
				aBP_CN[i].offs -= s;
		}
	}
}

bool PushBackBPCondition(uint16_t index, const uint8_t *cond, uint16_t length)
{
	if (length && length < (MAX_BP_CONDITIONS - nBP_EX_Len)) {
		memcpy(aBP_EX + nBP_EX_Len, cond, length);
		aBP_CN[index].offs = nBP_EX_Len;
		aBP_CN[index].size = length;
		nBP_EX_Len += length;
		return true;
	}
	return false;
}

bool SetBPCondition(uint32_t id, const uint8_t *condition, uint16_t length)
{
	uint16_t idx = 0xffff;
	for (uint16_t i = 0; i < nBP; i++) {
		if (aBP_ID[i] == id) {
			IBMutexLock(&mutexBP);
			if (length == aBP_CN[i].size) {
				memcpy(aBP_EX + aBP_CN[i].offs, condition, length);
				IBMutexRelease(&mutexBP);
				return true;
			}
			EraseBPCondition(i);
			idx = i;
			break;
		}
	}
	bool ret = false;
	if (idx != 0xffff) {
		ret = PushBackBPCondition(idx, condition, length);
		IBMutexRelease(&mutexBP);
	}
	return ret;
}

void ClearBPCondition(uint32_t id)
{
	for (uint16_t i = 0; i < nBP; i++) {
		if (aBP_ID[i] == id) {
			IBMutexLock(&mutexBP);
			EraseBPCondition(i);
			IBMutexRelease(&mutexBP);
			break;
		}
	}
}

void ClearAllPCBreakpoints()
{
	IBMutexLock(&mutexBP);
	uint16_t nBP = 0;		// total number of PC breakpoints
	uint16_t nBP_PC = 0;	// number of PC breakpoints
	uint16_t nBP_DS = 0;	// number of disabled breakpoints
	uint16_t nBP_EX_Len = 0;
	uint32_t nBP_NextID = 0;
	uint16_t bStopCPU = 0;
	uint16_t bCPUIRQ = 0;
	uint16_t bCPUNMI = 0;
	IBMutexRelease(&mutexBP);
}

void SwapBPSlots(uint16_t b, uint16_t s)
{
	if (s != b) {
		uint32_t id = aBP_ID[b]; aBP_ID[b] = aBP_ID[s]; aBP_ID[s] = id;
		uint16_t t = aBP_PC[b];	aBP_PC[b] = aBP_PC[s]; aBP_PC[s] = t;
		t = aBP_CN[b].offs; aBP_CN[b].offs = aBP_CN[s].offs; aBP_CN[s].offs = t;
		t = aBP_CN[b].size; aBP_CN[b].size = aBP_CN[s].size; aBP_CN[s].size = t;
	}
}

void MoveBPSlots(uint16_t s, uint16_t d)
{
	if (s != d) {
		aBP_PC[d] = aBP_PC[s];
		aBP_ID[d] = aBP_ID[s];
		aBP_CN[d] = aBP_CN[s];
	}
}

// if delete - return index of breakpoint, otherwise ~0
uint32_t TogglePCBreakpoint(uint16_t addr)
{
	IBMutexLock(&mutexBP);
	uint16_t nBP_T = nBP + nBP_DS;
	for (int b = 0; b < nBP_T; b++) {
		if (aBP_PC[b] == addr) {
			uint32_t id = aBP_ID[b];
			EraseBPCondition(b);
			if (b >= nBP) {
				nBP_DS--;
				MoveBPSlots(nBP + nBP_DS, b);
			} else {
				nBP--;
				MoveBPSlots(nBP, b);
				if (nBP_DS)
					MoveBPSlots(nBP + nBP_DS, nBP);
			}
			IBMutexRelease(&mutexBP);
			return id;
		}
	}
	if (nBP < MAX_PC_BREAKPOINTS) {
		if (nBP_DS)
			MoveBPSlots(nBP, nBP + nBP_DS);
		aBP_ID[nBP] = nBP_NextID++;
		aBP_CN[nBP].size = 0;
		aBP_PC[nBP++] = addr;
	}
	IBMutexRelease(&mutexBP);
	return ~0UL;
}

uint32_t SetPCBreakpoint(uint16_t addr)
{
	uint32_t ret = ~0UL;
	IBMutexLock(&mutexBP);

	uint16_t nBP_T = nBP + nBP_DS;
	for (int b = 0; b < nBP_T; b++) {
		if (aBP_PC[b] == addr) {
			IBMutexRelease(&mutexBP);
			return aBP_ID[b];
		}
	}
	if (nBP < MAX_PC_BREAKPOINTS) {
		if (nBP_DS)
			MoveBPSlots(nBP, nBP + nBP_DS);
		ret = aBP_ID[nBP] = nBP_NextID++;
		aBP_CN[nBP].size = 0;
		aBP_PC[nBP++] = addr;
	}
	IBMutexRelease(&mutexBP);
	return ret;
}

void ClearPCBreakpoint(uint16_t addr)
{
	IBMutexLock(&mutexBP);

	for (int b = 0; b < nBP; b++) {
		if (aBP_PC[b] == addr) {
			MoveBPSlots(nBP - 1, b);
			nBP--;
		}
	}
	IBMutexRelease(&mutexBP);
}

bool GetBreakpointAddrByID(uint32_t id, uint16_t &addr)
{
	uint16_t nBP_T = nBP + nBP_DS;
	for (int b = 0; b < nBP_T; b++) {
		if (aBP_ID[b] == id) {
			addr = aBP_PC[b];
			return true;
		}
	}
	return false;
}

void RemoveBreakpointByID(uint32_t id)
{
	IBMutexLock(&mutexBP);
	for (int b = 0; b < (nBP + nBP_DS); b++) {
		if (aBP_ID[b] == id || id == ~0UL) {
			EraseBPCondition(b);
			if (b < nBP) {
				nBP--;
				MoveBPSlots(nBP, b);
				if (nBP_DS)
					MoveBPSlots(nBP + nBP_DS, nBP);
			} else {
				nBP_DS--;
				if (b != nBP_DS)
					MoveBPSlots(b, nBP + nBP_DS);
			}
		}
	}
	IBMutexRelease(&mutexBP);
}

bool EnableBPByID(uint32_t id, bool enable)
{
	IBMutexLock(&mutexBP);
	if (enable) {
		uint16_t nBP_T = nBP + nBP_DS;
		for (int b = nBP; b < nBP_T; b++) {
			if (aBP_ID[b] == id) {
				SwapBPSlots(b, nBP);
				nBP++;
				nBP_DS--;
				IBMutexRelease(&mutexBP);
				return true;
			}

		}
	} else {
		for (uint16_t b = 0; b < nBP; b++) {
			if (aBP_ID[b] == id) {
				nBP--;
				nBP_DS++;
				SwapBPSlots(b, nBP);
				IBMutexRelease(&mutexBP);
				return true;
			}
		}
	}
	IBMutexRelease(&mutexBP);
	return false;
}

int InstructionBytes(uint16_t addr, bool illegals)
{
	const dismnm *opcodes = a6502_ops;
	unsigned char op = Get6502Byte(addr);
	bool not_valid = opcodes[op].mnemonic == mnm_inv || (!illegals && opcodes[op].mnemonic >= mnm_wdc_and_illegal_instructions);
	int arg_size = not_valid ? 0 : opcodes[op].arg_size;;

	return arg_size + 1;
}

int InstrRef(uint16_t pc, char* buf, size_t bufSize)
{
	const dismnm *opcodes = a6502_ops;
	uint8_t m = opcodes[Get6502Byte(pc)].addrMode;

	switch (m) {
		case AM_ZP_REL_X:
		{	// 0 ($12:x)
			uint8_t z = Get6502Byte(pc +1) + GetRegs().X;
			uint16_t addr = Get6502Byte(z) + ((uint16_t)Get6502Byte((z + 1) & 0xff) << 8);
			return sprintf_s(buf, bufSize, "(%04x)=%02x", addr, Get6502Byte(addr));
		}
		case AM_ZP:
		{	// 1 $12
			uint8_t z = Get6502Byte(pc +1);
			return sprintf_s(buf, bufSize, "(%02x)=%02x", z, Get6502Byte(z));
		}
		case AM_ABS:
		{	// 3 $1234
			uint16_t addr = Get6502Byte(pc+1) + ((uint16_t)Get6502Byte((pc + 2)) << 8);
			return sprintf_s(buf, bufSize, "(%04x)=%02x", addr, Get6502Byte(addr));
		}
		case AM_ZP_Y_REL:
		{	// 4 ($12):y
			uint8_t z = Get6502Byte(pc + 1);
			uint16_t addr = Get6502Byte(z) + GetRegs().Y + ((uint16_t)Get6502Byte((z + 1) & 0xff) << 8);
			return sprintf_s(buf, bufSize, "(%04x)=%02x", addr, Get6502Byte(addr));
		}
		case AM_ZP_X:
		{	// 5 $12:x
			uint8_t z = Get6502Byte(pc + 1) + GetRegs().X;
			return sprintf_s(buf, bufSize, "(%02x)=%02x", z, Get6502Byte(z));
		}
		case AM_ABS_Y:
		{	// 6 $1234:y
			uint16_t addr = Get6502Byte(pc + 1) + ((uint16_t)Get6502Byte((pc + 2)) << 8) + GetRegs().Y;
			return sprintf_s(buf, bufSize, "(%04x)=%02x", addr, Get6502Byte(addr));
		}
		case AM_ABS_X:
		{	// 7 $1234:x
			uint16_t addr = Get6502Byte(pc + 1) + ((uint16_t)Get6502Byte((pc + 2)) << 8) + GetRegs().X;
			return sprintf_s(buf, bufSize, "(%04x)=%02x", addr, Get6502Byte(addr));
		}
		case AM_REL:
		{	// 8 ($1234)
			uint16_t addr = Get6502Byte(pc + 1) + ((uint16_t)Get6502Byte((pc + 2)) << 8);
			uint16_t rel = Get6502Byte(addr) + ((uint16_t)Get6502Byte(((addr + 1) & 0xff) | (addr & 0xff00)) << 8);
			return sprintf_s(buf, bufSize, "(%04x)=%04x", addr, rel);
		}
		case AM_ACC:
		{	// 9 A
			return sprintf_s(buf, bufSize, "A=%02x", GetRegs().A);
		}
		case AM_ZP_REL_Y:
		{	// c ($12:y)
			uint8_t z = Get6502Byte(pc + 1) + GetRegs().Y;
			uint16_t addr = Get6502Byte(z) + ((uint16_t)Get6502Byte((z + 1) & 0xff) << 8);
			return sprintf_s(buf, bufSize, "(%04x)=%02x", addr, Get6502Byte(addr));
		}
		case AM_ZP_Y:
		{	// d $12:x
			uint8_t z = Get6502Byte(pc + 1) + GetRegs().Y;
			return sprintf_s(buf, bufSize, "(%02x)=%02x", z, Get6502Byte(z));
		}
	}
	buf[0] = 0;
	return 0;
}

// disassemble one instruction at addr into the dest string and return number of bytes for instruction
int Disassemble(uint16_t addr, char *dest, int left, int &chars, int& branchTrg, bool showBytes, bool illegals, bool showLabels)
{
	const dismnm *opcodes = a6502_ops;
	int bytes = 1;
	int orig_left = left;
	unsigned char op = Get6502Byte(addr);
	bool not_valid = opcodes[op].mnemonic == mnm_inv || (!illegals && opcodes[op].mnemonic >= mnm_wdc_and_illegal_instructions);

	int arg_size = not_valid ? 0 : opcodes[op].arg_size;;
	int mode = not_valid ? AM_NON : opcodes[op].addrMode;
	bytes += arg_size;

	int prtd = 0, pos = 0;

	if (showBytes) {
		for (uint16_t b = 0; b <= (uint16_t)arg_size; b++) {
			prtd = sprintf_s(dest, left, "%02x ", Get6502Byte(addr + b));
			dest += prtd;
			left -= prtd;
			pos += prtd;
		}
		for (uint16_t b = pos; left && b < 9; b++) {
			*dest++ = ' ';
			left--;
		}
		pos = 9;
	}

	if (not_valid) {
		prtd = sprintf_s(dest, left, "dc.b %02x ", Get6502Byte(addr));
		dest += prtd;
		left -= prtd;
		pos += prtd;
	} else {
		addr++;
		const char *mnemonic = zsMNM[opcodes[op].mnemonic];
		uint16_t arg;
		const char *label;
		//char label8[256];
		switch (mode) {
			case AM_ABS:		// 3 $1234
			case AM_ABS_Y:		// 6 $1234,y
			case AM_ABS_X:		// 7 $1234,x
			case AM_REL:		// 8 ($1234)
				arg = (uint16_t)Get6502Byte(addr) | ((uint16_t)Get6502Byte(addr + 1)) << 8;
				if (op == 0x20 || op == 0x4c) { branchTrg = arg; }
				label = showLabels ? GetSymbol(arg) : nullptr;
				if (label) {
					//size_t newlen = 0;
					//wcstombs_s(&newlen, label8, label, sizeof(label8)-1);
					prtd = sprintf_s(dest, left, aAddrModeLblFmt[mode], mnemonic, label, arg);
				} else
					prtd = sprintf_s(dest, left, aAddrModeFmt[mode], mnemonic, arg);
				break;

			case AM_BRANCH:		// beq $1234
				arg = addr + 1 + (char)Get6502Byte(addr);
				branchTrg = arg;
				label = showLabels ? GetSymbol(arg) : nullptr;
				if (label) {
					//size_t newlen = 0;
					//wcstombs_s(&newlen, label8, label, sizeof(label8)-1);
					prtd = sprintf_s(dest, left, aAddrModeLblFmt[mode], mnemonic, label, arg);
				} else
					prtd = sprintf_s(dest, left, aAddrModeFmt[mode], mnemonic, arg);
				break;

			default:
				prtd = sprintf_s(dest, left, aAddrModeFmt[mode], mnemonic,
								 Get6502Byte(addr), Get6502Byte(addr + 1));
				break;
		}

		dest += prtd;
		left -= prtd;
	}
	chars = orig_left - left;
	return 1 + arg_size;
}

int Assemble(char *cmd, uint16_t addr)
{
	// skip initial stuff
	while (*cmd && *cmd < 'A') cmd++;

	const char *instr = cmd;
	while (*cmd && *cmd >= 'A') cmd++;

	size_t instr_len = cmd - instr;
	int mnm = -1;

	if (!instr_len) { return 0; }

	while (*cmd && *cmd <= ' ') cmd++;

	for (int i = 0; i < (sizeof(zsMNM) / sizeof(zsMNM[0])); i++) {
		if (_strnicmp(zsMNM[i], instr, instr_len) == 0) {
			mnm = i;
			break;
		}
	}

	if (mnm < 0)
		return 0;	// instruction not found

	// get valid address modes
	int addr_modes = 0;
	int addr_mode_mask = 0;
	uint8_t op_code_instr[AM_COUNT] = { (uint8_t)0xff };
	for (int i = 0; i < 256; i++) {
		if (a6502_ops[i].mnemonic == mnm) {
			addr_modes++;
			addr_mode_mask |= 1 << a6502_ops[i].addrMode;
			op_code_instr[a6502_ops[i].addrMode] = (uint8_t)i;
		}
	}

	// get address mode
	bool rel = false;
	bool abs = false;
	bool hex = false;
	bool imm = false;
	bool imp = false;
	bool ix = false;
	bool iy = false;
	int mode = -1;
	switch (*cmd) {
		case 0: imp = true; break;
		case '#': imm = true; ++cmd; break;
		case '$': abs = true; hex = true; ++cmd; break;
		case '(': rel = true; ++cmd; break;
		case 'a':
		case 'A': imp = (cmd[1] <= ' ' || cmd[1] == ';'); break;
	}
	int arg = -1;
	if (!imp) {
		if ((imm || rel) && *cmd == '$') {
			hex = true;
			cmd++;
		}
		if (!hex && !imm && !rel)
			abs = true;
		arg = strtol(cmd, &cmd, hex ? 16 : 10);
		while (*cmd && *cmd <= ' ') cmd++;
		if (*cmd == ',') {
			++cmd;
			while (*cmd && *cmd <= ' ') cmd++;
			if (*cmd == 'x' || *cmd == 'X')
				ix = true;
			else if ((!rel || mnm == mnm_stx || mnm == mnm_ldx) && (*cmd == 'y' || *cmd == 'Y'))
				iy = true;
			else
				return 0;
		}
		if (*cmd == ')') {
			++cmd;
			if (!rel)
				return 0;
		}
		if (*cmd == ',' && rel) {
			++cmd;
			while (*cmd && *cmd <= ' ') cmd++;
			if (*cmd == 'y' || *cmd == 'Y')
				iy = true;
			else
				return 0;
		}
		if (rel)
			mode = (!ix && !iy) ? AM_REL : (ix ? AM_ZP_REL_X : AM_ZP_Y_REL);
		else if (imm)
			mode = AM_IMM;
		else if (abs) {
			if (addr_mode_mask & (1 << AM_BRANCH)) {
				mode = AM_BRANCH;
				arg -= addr + 2;
				if (arg < -128 || arg>127)
					return 0;
			} else if (arg < 0x100 && mnm == mnm_stx && iy)
				mode = AM_ZP_Y;
			else
				mode = (arg < 0x100) ? (ix ? AM_ZP_X : (iy ? AM_ZP_Y : AM_ZP)) :
				(ix ? AM_ABS_X : (iy ? AM_ABS_Y : AM_ABS));
		}
	} else
		mode = (addr_mode_mask & (1 << AM_ACC)) ? AM_ACC : AM_NON;

	if (mode < 0 || !(addr_mode_mask & (1 << mode)))
		return 0;	// invalid mode

	uint8_t op = op_code_instr[mode];
	int bytes = a6502_ops[op].arg_size;
	Set6502Byte(addr++, op);
	if (bytes) {
		Set6502Byte(addr++, (uint8_t)arg);
		if (bytes > 1)
			Set6502Byte(addr++, (uint8_t)(arg >> 8));
	}
	return bytes + 1;
}


