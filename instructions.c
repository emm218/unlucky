#include "instructions.h"

struct instruction instruction_set[0x100] = {
	{ BRK, IMP },
	{ ORA, IDX },
	{},
	{},
	{},
	{ ORA, ZRP },
	{ ASL, ZRP },
	{},
	{ PHP, IMP },
	{ ORA, IMM },
	{ ASL, ACC },
	{},
	{},
	{ ORA, ABS },
	{ ASL, ABS },
	{},
	{ BPL, REL },
	{ ORA, IDY },
	{},
	{},
	{},
	{ ORA, ZPX },
	{ ASL, ZPX },
	{},
	{ CLC, IMP },
	{ ORA, ABY },
	{},
	{},
	{},
	{ ORA, ABX },
	{ ASL, ABX },
	{},
	{ JSR, ABS },
	{ AND, IDX },
	{},
	{},
	{ BIT, ZRP },
	{ AND, ZRP },
	{ ROL, ZRP },
	{},
	{ PLP, IMP },
	{ AND, IMM },
	{ ROL, ACC },
	{},
	{ BIT, ABS },
	{ AND, ABS },
	{ ROL, ABS },
	{},
	{ BMI, REL },
	{ AND, IDY },
	{},
	{},
	{},
	{ AND, ZPX },
	{ ROL, ZPX },
	{},
	{ SEC, IMP },
	{ AND, ZPY },
	{},
	{},
	{},
	{ AND, ZPX },
	{ ROL, ZPX },
	{},
	{ RTI, IMP },
	{ EOR, IDX },
	{},
	{},
	{},
	{ EOR, ZRP },
	{ LSR, ZRP },
	{},
	{ PHA, IMP },
	{ EOR, IMM },
	{ LSR, ACC },
	{},
	{ JMP, ABS },
	{ EOR, ABS },
	{ LSR, ABS },
	{},
	{ BVC, REL },
	{ EOR, IDY },
	{},
	{},
	{},
	{ EOR, ZPX },
	{ LSR, ZPX },
	{},
	{ CLI, IMP },
	{ EOR, ABY },
	{},
	{},
	{},
	{ EOR, ABX },
	{ LSR, ABX },
	{},
	{ RTS, IMP },
	{ ADC, IDX },
	{},
	{},
	{},
	{ ADC, ZRP },
	{ ROR, ZRP },
	{},
	{ PLA, IMP },
	{ ADC, IMM },
	{ ROR, ACC },
	{},
	{ JMP, IND },
	{ ADC, ABS },
	{ ROR, ABS },
	{},
	{ BVS, REL },
	{ ADC, IDY },
	{},
	{},
	{},
	{ ADC, ZPX },
	{ ROR, ZPX },
	{},
	{ SEI, IMP },
	{ ADC, ABY },
	{},
	{},
	{},
	{ ADC, ABX },
	{ ROR, ABX },
	{},
	{},
	{ STA, IDX },
	{},
	{},
	{ STY, ZRP },
	{ STA, ZRP },
	{ STX, ZRP },
	{},
	{ DEY, IMP },
	{},
	{ TXA, IMP },
	{},
	{ STY, ABS },
	{ STA, ABS },
	{ STX, ABS },
	{},
	{ BCC, REL },
	{ STA, IDY },
	{},
	{},
	{ STY, ZPX },
	{ STA, ZPX },
	{ STX, ZPY },
	{},
	{ TYA, IMP },
	{ STA, ABY },
	{ TXS, IMP },
	{},
	{},
	{ STA, ABX },
	{},
	{},
	{ LDY, IMM },
	{ LDA, IDX },
	{ LDX, IMM },
	{},
	{ LDY, ZRP },
	{ LDA, ZRP },
	{ LDX, ZRP },
	{},
	{ TAY, IMP },
	{ LDA, IMM },
	{ TAX, IMP },
	{},
	{ LDY, ABS },
	{ LDA, ABS },
	{ LDX, ABS },
	{},
	{ BCS, REL },
	{ LDA, IDY },
	{},
	{},
	{ LDY, ZPX },
	{ LDA, ZPX },
	{ LDX, ZPY },
	{},
	{ CLV, IMP },
	{ LDA, ABY },
	{ TSX, IMP },
	{},
	{ LDY, ABX },
	{ LDA, ABX },
	{ LDX, ABY },
	{},
	{ CPY, IMM },
	{ CMP, ZPX },
	{},
	{},
	{ CPY, ZRP },
	{ CMP, ZRP },
	{ DEC, ZRP },
	{},
	{ INY, IMP },
	{ CMP, IMM },
	{ DEX, IMP },
	{},
	{ CPY, ABS },
	{ CMP, ABS },
	{ DEC, ABS },
	{},
	{ BNE, REL },
	{ CMP, IDY },
	{},
	{},
	{},
	{ CMP, ZPX },
	{ DEC, ZPX },
	{},
	{ CLD, IMP },
	{ CMP, ABY },
	{},
	{},
	{},
	{ CMP, ABX },
	{ DEC, ABX },
	{},
	{ CPX, IMM },
	{ SBC, IDX },
	{},
	{},
	{ CPX, ZRP },
	{ SBC, ZRP },
	{ INC, ZRP },
	{},
	{ INX, IMP },
	{ SBC, IMM },
	{ NOP, IMP },
	{},
	{ CPX, ABS },
	{ SBC, ABS },
	{ INC, ABS },
	{},
	{ BEQ, REL },
	{ SBC, IDY },
	{},
	{},
	{},
	{ SBC, ZPX },
	{ INC, ZPX },
	{},
	{ SED, IMP },
	{ SBC, ABY },
	{},
	{},
	{},
	{ SBC, ABX },
	{ INC, ABX },
	{},
};

char *opcodes[] = {
	"",
	"adc",
	"and",
	"asl",
	"bcc",
	"bcs",
	"beq",
	"bit",
	"bmi",
	"bne",
	"bpl",
	"brk",
	"bvc",
	"bvs",
	"clc",
	"cld",
	"cli",
	"clv",
	"cmp",
	"cpx",
	"cpy",
	"dec",
	"dex",
	"dey",
	"eor",
	"inc",
	"inx",
	"iny",
	"jmp",
	"jsr",
	"lda",
	"ldx",
	"ldy",
	"lsr",
	"nop",
	"ora",
	"pha",
	"php",
	"pla",
	"plp",
	"rol",
	"ror",
	"rti",
	"rts",
	"sbc",
	"sec",
	"sed",
	"sei",
	"sta",
	"stx",
	"sty",
	"tax",
	"tay",
	"tsx",
	"txa",
	"txs",
	"tya",
};