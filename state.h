#ifndef STATE
#define STATE
#include <vector>
#include <cstdint>
#include <iostream>
#include "control.h"
// Pipeline registers implementation

// IFID Pipeline register, only contains instruction and pc + 4
struct IFID {
	bool empty;
	uint32_t PC;
	uint32_t instruction;
	uint32_t opcode;
	uint32_t Rs;
	uint32_t Rt;
	uint32_t Rd;
	uint32_t Imm;
	uint32_t Shamt;
	uint32_t Funct;
	bool branchPred;

	void print() {
		cout << "\n";
		cout << "IFID: " << "\n";
		cout << "EMPTY: " << empty << "\n";
		cout << "INSTRUCTION: " << instruction << "\n";
		cout << "PC: " << PC << "\n";
		cout << "RS: " << Rs << "\n";
		cout << "RT: " << Rt << "\n";
		cout << "RD: " << Rd << "\n";
		cout << "OPCODE: " << opcode << "\n";
		cout << "IMM: " << Imm << "\n";
		cout << "SHAMT: " << Shamt << "\n";
		cout << "FUNCT: " << Funct << "\n";
		cout << "BRANCHPRED: " << branchPred << "\n";
		cout << "\n";
	}
};

// IDEX Pipeline register
struct IDEX {
	bool empty;
	control_t control;
	uint32_t instruction;
	uint32_t PC;
	uint32_t readData1;
	uint32_t readData2;
	uint32_t Rs;
	uint32_t Rt;
	uint32_t Rd;
	uint32_t opcode;
	uint32_t Imm;
	uint32_t signExtendImm;
	uint32_t Shamt;
	uint32_t Funct;
	bool branchPred;

	void print() {
		cout << "\n";
		cout << "IDEX: " << "\n";
		cout << "EMPTY: " << empty << "\n";
		cout << "CONTROL: " << "\n";
		control.print();
		cout << "INSTRUCTION: " << instruction << "\n";
		cout << "PC: " << PC << "\n";
		cout << "READDATA1: " << readData1 << "\n";
		cout << "READDATA2: " << readData2 << "\n";
		cout << "RS: " << Rs << "\n";
		cout << "RT: " << Rt << "\n";
		cout << "RD: " << Rd << "\n";
		cout << "OPCODE: " << opcode << "\n";
		cout << "IMM: " << Imm << "\n";
		cout << "SIGNEXTENDEDIMM: " << signExtendImm << "\n";
		cout << "SHAMT: " << Shamt << "\n";
		cout << "FUNCT: " << Funct << "\n";
		cout << "BRANCHPRED: " << branchPred << "\n";
	}
};

// EXMEM Pipeline register
struct EXMEM {
	bool empty;
	control_t control;
	uint32_t instruction;
	uint32_t PC;
	uint32_t PCbranch;
	uint32_t zeroFlag;
	uint32_t ALUresult;
	uint32_t readData1;
	uint32_t readData2;
	uint32_t Rt;
	uint32_t Rd;
	uint32_t regDestination;
	uint32_t signExtendImm;
	bool jumpReg;
	bool PCsrc;
	bool branchPred;

	void print() {
		cout << "\n";
		cout << "EXMEM: " << "\n";
		cout << "EMPTY: " << empty << "\n";
		cout << "CONTROL: " << "\n";
		control.print();
		cout << "INSTRUCTION: " << instruction << "\n";
		cout << "PC: " << PC << "\n";
		cout << "PCBRANCH: " << PCbranch << "\n";
		cout << "ZEROFLAG: " << zeroFlag << "\n";
		cout << "ALURESULT: " << ALUresult << "\n";
		cout << "READDATA1: " << readData1 << "\n";
		cout << "READDATA2: " << readData2 << "\n";
		cout << "RT: " << Rt << "\n";
		cout << "RD: " << Rd << "\n";
		cout << "REGDESTINATION: " << regDestination << "\n";
		cout << "SIGNEXTENDEDIMM: " << signExtendImm << "\n";
		cout << "JUMPREG: " << jumpReg << "\n";
		cout << "PCSRC: " << PCsrc << "\n";
		cout << "BRANCHPRED: " << branchPred << "\n";
	}
};

// MEMWB Pipeline register
struct MEMWB {
	bool empty;
	control_t control;
	uint32_t instruction;
	uint32_t Rt;
	uint32_t Rd;
	uint32_t memReadData;
	uint32_t ALUresult;
	uint32_t regDestination;
	uint32_t PC;
	bool PCsrc;
	bool jumpReg;

	void print() {
		cout << "\n";
		cout << "MEMWB: " << "\n";
		cout << "EMPTY: " << empty << "\n";
		cout << "CONTROL: " << "\n";
		control.print();
		cout << "ALURESULT: " << ALUresult << "\n";
		cout << "MEMREADDATA: " << memReadData << "\n";
		cout << "INSTRUCTION: " << instruction << "\n";
		cout << "REGDESTINATION: " << regDestination << "\n";
		cout << "RT: " << Rt << "\n";
		cout << "RD: " << Rd << "\n";
		cout << "JUMPREG: " << jumpReg << "\n";
		cout << "PC: " << PC << "\n";
		cout << "PCSrc: " << PCsrc << "\n";
	}
};

#endif