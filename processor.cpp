#include <cstdint>
#include <iostream>
#include "memory.h"
#include "reg_file.h"
#include "ALU.h"
#include "control.h"
#include "state.h"

using namespace std;

// Sample processor main loop for a single-cycle processor
void single_cycle_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc) {
    // Initialize ALU
    ALU alu;
    // Initialize Control
    control_t control = {.reg_dest = false, .jump = false, .branch = false, .mem_read = false, .mem_to_reg = false, .ALU_op = 3, .mem_write = false, .ALU_src = false, .reg_write = false, .branchNotEqual = false, .jumpLink = false, .loadUpperImm = false, .storeByte = false, .storeHalfWord = false, .loadByteU = false, .loadHalfWordU = false};
    uint32_t num_cycles = 0;
    uint32_t num_instrs = 0; 

    while (reg_file.pc != end_pc) {
        // fetch
        uint32_t instruction;
        memory.access(reg_file.pc, instruction, 0, 1, 0);
        uint32_t opcode = instruction >> 26; //Instruction[31-26]
        uint32_t Rs = (instruction >> 21) & 0b11111; //Instruction [25-21]
        uint32_t Rt = (instruction >> 16) & 0b11111; //Instruction [20-16]
        uint32_t Rd = (instruction >> 11) & 0b11111; //Instruction [15-11]
        uint32_t Imm = instruction & 0b1111111111111111; //Instruction [15-0]
        uint32_t Shamt = (instruction >> 6) &0b11111; //Instruction [10-6]
        uint32_t Funct = instruction & 0b111111; //Instruction [5-0]

        // increment pc
        reg_file.pc += 4;

        // decode into contol signals
        control.decode(opcode);
        control.print(); // used for autograding

        // Read from reg file
        uint32_t readData1;
        uint32_t readData2;
        reg_file.access(Rs, Rt, readData1, readData2, 0, 0, 0);

        //Sign-extend
        uint32_t signExtend;
        if (Imm >> 15 == 1) {
            signExtend = Imm | 0b11111111111111110000000000000000;
        }
        else {
            signExtend = Imm;
        }

        // Execution 
        alu.generate_control_inputs(control.ALU_op, Funct, opcode);

        //Special cases: Andi and Ori
        if (alu.zeroExtend == true) {
            signExtend = Imm;
        }

        //Shifts
        if (alu.shift == true) {
            readData1 = Shamt;
        }

        //ALU
        uint32_t alu_result;
        uint32_t zeroFlag = 0;
        if (control.ALU_src == 0) {
            alu_result = alu.execute(readData1, readData2, zeroFlag);
        }
        else if (control.ALU_src == 1) {
            alu_result = alu.execute(readData1, signExtend, zeroFlag);
        }
        
        //Branch
        uint32_t jumpAddress = instruction & 0b11111111111111111111111111; //Instruction [25-0]
        if (control.jump == 1) {
            if (control.jumpLink == 1) {
                uint32_t temp = reg_file.pc + 4; //PC + 8
                reg_file.access(Rs, Rt, readData1, readData2, 31, control.reg_write, temp); //R31 = PC + 8
            }
            jumpAddress = jumpAddress << 2;
            uint32_t temp = reg_file.pc & 0b11110000000000000000000000000000; //PC + 4 [31-28]
            jumpAddress += temp; //Jump Address [31=0]
            reg_file.pc = jumpAddress; //next PC jump address
        }
        else if (control.branch == 1 && control.branchNotEqual == 0 && zeroFlag == 1) { //next PC Address MUX
            reg_file.pc += signExtend << 2;
        }
        else if (control.branch == 1 && control.branchNotEqual == 1 && zeroFlag == 0) { //controls BNE MUX
            reg_file.pc += signExtend << 2;
        }
        else if (alu.jumpReg == true) { //controls Jump Register MUX
            reg_file.pc = readData1;
        }

        //Store Byte and Halfword
        uint32_t memReadResult;
        uint32_t notReadingDataAnyways;
        if (control.storeByte == 1) {
            readData2 = readData2 & 0b11111111; //chops 24 bits off from read data 2
            memory.access(alu_result, memReadResult, 0, true, 0); //M[address + displacement]
            memReadResult = memReadResult & 0b11111111111111111111111100000000; //M[address + displacement](31:8)
            memReadResult = memReadResult | readData2;
            memory.access(alu_result, notReadingDataAnyways, memReadResult, 0, 1); //Store M[address + displacement](7:0) = Rt(7:0)
        }
        else if (control.storeHalfWord == 1) {
            readData2 = readData2 & 0b1111111111111111; //chops 16 bits off from read data 2
            memory.access(alu_result, memReadResult, 0, true, 0); //M[address + displacement]
            memReadResult = memReadResult & 0b11111111111111110000000000000000; //M[address + displacement](31:16)
            memReadResult = memReadResult | readData2;
            memory.access(alu_result, notReadingDataAnyways, memReadResult, 0, 1); //Store M[address + displacement](15:0) = Rt(15:0)
        }
        else {
            //Memory access
            memory.access(alu_result, memReadResult, readData2, control.mem_read, control.mem_write); //Load and Store
        }

        //Load Byte and Halfword
        if (control.loadByteU == 1) {
            memReadResult = memReadResult & 0b11111111; //R[rt]=M[R[rs]+SignExtImm](7:0)
        }
        else if (control.loadHalfWordU == 1) {
            memReadResult = memReadResult & 0b1111111111111111; //R[rt]=M[R[rs]+SignExtImm](15:0)
        }

        //Write Back
        if (control.loadUpperImm == 1) { //Load Upper Immediate
            uint32_t temp = Imm << 16;
            reg_file.access(Rs, Rt, readData1, readData2, Rt, control.reg_write, temp);
        }
        else if (control.jumpLink == 0 && alu.jumpReg == 0 && control.branch == 0 && control.branchNotEqual == 0) { //Prevents Jump and Link from working correctly
            if (control.mem_to_reg == 1) { //write back memory read result
                if (control.reg_dest == 0) { //use Rt
                    reg_file.access(Rs, Rt, readData1, readData2, Rt, control.reg_write, memReadResult);
                }
                else if (control.reg_dest == 1) { //use Rd
                    reg_file.access(Rs, Rt, readData1, readData2, Rd, control.reg_write, memReadResult);
                }
            }
            else if (control.mem_to_reg == 0) { //write back ALU result
                if (control.reg_dest == 0) {
                    reg_file.access(Rs, Rt, readData1, readData2, Rt, control.reg_write, alu_result);
                }
                else if (control.reg_dest == 1) {
                    reg_file.access(Rs, Rt, readData1, readData2, Rd, control.reg_write, alu_result);
                }
            }
        }

        //Update the PC
        cout << "CYCLE" << num_cycles << "\n";
        reg_file.print(); // used for automated testing
        num_cycles++;
        num_instrs++;
    }
    cout << "CPI = " << (double)num_cycles/(double)num_instrs << "\n";
}

void pipelined_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc) {
    // Initialize ALU
    ALU alu;
    // Initialize Pipeline registers
    IFID ifid = {.empty = true, .PC = 0, .instruction = 0, .opcode = 0, .Rs = 0, .Rt = 0, .Rd = 0, .Imm = 0, .Shamt = 0, .Funct = 0};
	IDEX idex = {.empty = true, .control = {.reg_dest = false, .jump = false, .branch = false, .mem_read = false, .mem_to_reg = false, .ALU_op = 3, .mem_write = false, .ALU_src = false, .reg_write = false, .branchNotEqual = false, .jumpLink = false, .loadUpperImm = false, .storeByte = false, .storeHalfWord = false, .loadByteU = false, .loadHalfWordU = false}, .instruction = 0, .PC = 0, .readData1 = 0, .readData2 = 0, .Rs = 0, .Rt = 0, .Rd = 0, .opcode = 0, .Imm = 0, .signExtendImm = 0, .Shamt = 0, .Funct = 0};
    EXMEM exmem = {.empty = true, .control = {.reg_dest = false, .jump = false, .branch = false, .mem_read = false, .mem_to_reg = false, .ALU_op = 3, .mem_write = false, .ALU_src = false, .reg_write = false, .branchNotEqual = false, .jumpLink = false, .loadUpperImm = false, .storeByte = false, .storeHalfWord = false, .loadByteU = false, .loadHalfWordU = false}, .instruction = 0, .PC = 0, .PCbranch = 0, .zeroFlag = 0, .ALUresult = 0, .readData1 = 0, .readData2 = 0, .Rt = 0, .Rd = 0, .regDestination = 0, .signExtendImm = 0, .jumpReg = false, .PCsrc = false};
    MEMWB memwb = {.empty = true, .control = {.reg_dest = false, .jump = false, .branch = false, .mem_read = false, .mem_to_reg = false, .ALU_op = 3, .mem_write = false, .ALU_src = false, .reg_write = false, .branchNotEqual = false, .jumpLink = false, .loadUpperImm = false, .storeByte = false, .storeHalfWord = false, .loadByteU = false, .loadHalfWordU = false}, .instruction = 0, .Rt = 0, .Rd = 0, .memReadData = 0, .ALUresult = 0, .regDestination = 0, .PC = 0, .PCsrc = false, .jumpReg = false};
    uint32_t num_cycles = 0;
    uint32_t num_instrs = 0;

    while (true) {
        uint32_t committed_insts = 0;
		bool endIt = false;

        //Writeback -> Data writes into PC/Register file
        uint32_t dummy1 = 0;
        uint32_t dummy2 = 0;
		
		if (memwb.empty == true) {

		}else if (memwb.control.loadUpperImm == 1) { //Load Upper Immediate
            uint32_t temp = memwb.instruction << 16; //makes Imm most significant 16 bits
            reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, temp); //writes data back
        }
        else if (memwb.control.jumpLink == 0 && memwb.jumpReg == 0 && memwb.control.branch == 0 && memwb.control.branchNotEqual == 0) { //Prevents Jump and Link from working correctly
            if (memwb.control.mem_to_reg == 1) { //write back memory read result
                reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, memwb.memReadData); //RegDst determined in EX stage for Pipeline processor instead
            }
            else if (memwb.control.mem_to_reg == 0) { //write back ALU result
                reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, memwb.ALUresult); //RegDst determined in EX stage for Pipeline processor instead
            }
        }

        if (memwb.empty == false) {
            committed_insts = 1;
        }

		if (memwb.PC - 4 == end_pc) { //If the memwb's PC is the last instruction (memwb.PC carries PC+4, next instructions PC), end loop at the end of the cycle 
			endIt = true;
		}

        //Memory -> Read memory 
        uint32_t memReadResult = 0;
        uint32_t dummy3;
		
		if (exmem.empty == true) {}
		if (exmem.control.storeByte == 1) {
            uint32_t readData2Temp = exmem.readData2 & 0b11111111; //chops 24 bits off from read data 2
            memory.access(exmem.ALUresult, memReadResult, 0, true, 0); //M[address + displacement]

			if (exmem.ALUresult % 4 == 3) {
				memReadResult = memReadResult & 0b11111111111111111111111100000000; //M[address + displacement](31:8)
				memReadResult = memReadResult | readData2Temp;
			}
			else if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b11111111111111110000000011111111;
				readData2Temp = readData2Temp << 8;
				memReadResult = memReadResult | readData2Temp;
			}
			else if (exmem.ALUresult % 4 == 1) {
				memReadResult = memReadResult & 0b11111111000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult = memReadResult | readData2Temp;
			}
			else {
				memReadResult = memReadResult & 0b00000000111111111111111111111111;
				readData2Temp = readData2Temp << 24;
				memReadResult = memReadResult | readData2Temp;
			}
            memory.access(exmem.ALUresult, dummy3, memReadResult, 0, 1); //Store M[address + displacement](7:0) = Rt(7:0)
        }
        else if (exmem.control.storeHalfWord == 1) {
            uint32_t readData2Temp = exmem.readData2 & 0b1111111111111111; //chops 16 bits off from read data 2
            memory.access(exmem.ALUresult, memReadResult, 0, true, 0); //M[address + displacement]
			if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b11111111111111110000000000000000; //M[address + displacement](31:16)
				memReadResult = memReadResult | readData2Temp;
			}
			else {
				memReadResult = memReadResult & 0b00000000000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult = memReadResult | readData2Temp;
			}    
            memory.access(exmem.ALUresult, dummy3, memReadResult, 0, 1); //Store M[address + displacement](15:0) = Rt(15:0)
        }
        else {
            //Memory access
			memory.access(exmem.ALUresult, memReadResult, exmem.readData2, exmem.control.mem_read, exmem.control.mem_write); //Load and Store
        }
		
		if (exmem.control.loadByteU == 1) {
			if (exmem.ALUresult % 4 == 3) {
				memReadResult = memReadResult & 0b11111111; //R[rt]=M[R[rs]+SignExtImm](7:0)
			}
			else if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b1111111100000000;
				memReadResult = memReadResult >> 8;
			}
			else if (exmem.ALUresult % 4 == 1) {
				memReadResult = memReadResult & 0b111111110000000000000000;
				memReadResult = memReadResult >> 16;
			}
			else {
				memReadResult = memReadResult & 0b11111111000000000000000000000000;
				memReadResult = memReadResult >> 24;
			}
        }
        else if (exmem.control.loadHalfWordU == 1) {
			if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b1111111111111111; //R[rt]=M[R[rs]+SignExtImm](15:0)
			}
			else {
				memReadResult = memReadResult & 0b11111111111111110000000000000000;
				memReadResult = memReadResult >> 16;
			}
        }

        //MEMWB Pipeline -> Memory writes into pipeline
        memwb.empty = exmem.empty;
        memwb.control = exmem.control;
        memwb.instruction = exmem.instruction;
        memwb.Rt = exmem.Rt;
        memwb.Rd = exmem.Rd;
        memwb.memReadData = memReadResult;
        memwb.ALUresult = exmem.ALUresult;
        memwb.jumpReg = exmem.jumpReg;
        memwb.regDestination = exmem.regDestination;
		memwb.PC = exmem.PC;
		memwb.PCsrc = exmem.PCsrc;

		//MEM->MEM Forwarding
		if (memwb.control.mem_read == true && idex.control.mem_write == true) { //checking LW then SW dependency
			idex.readData2 = memwb.memReadData; //SW rt = LW rt
		}

        //Execute -> ALU
        alu.generate_control_inputs(idex.control.ALU_op, idex.Funct, idex.opcode);
        uint32_t signExtend = idex.signExtendImm;
        if (alu.zeroExtend == true) { //Special cases: Andi and Ori
            signExtend = idex.Imm;
        }
        uint32_t readData1Temp = idex.readData1;
        if (alu.shift == true) { //Shifts
            readData1Temp = idex.Shamt;
        }
        uint32_t alu_result = 0;
        uint32_t zeroFlag = 0;
		
		if (idex.empty) {

		}else if (idex.control.ALU_src == 0) {
            alu_result = alu.execute(readData1Temp, idex.readData2, zeroFlag);
        }
        else if (idex.control.ALU_src == 1) {
            alu_result = alu.execute(readData1Temp, signExtend, zeroFlag);
        }
		
		if (idex.control.reg_dest == 0) {
            exmem.regDestination = idex.Rt;
        }
        else{
            exmem.regDestination = idex.Rd;
        }
		
        //EXMEM Pipeline -> ALU result writes into pipeline
        exmem.empty = idex.empty;
        exmem.control = idex.control;
        exmem.instruction = idex.instruction;
        exmem.PC = idex.PC;
        exmem.PCbranch = (idex.signExtendImm << 2) + idex.PC;
        exmem.zeroFlag = zeroFlag;
        exmem.ALUresult = alu_result;
        exmem.readData1 = readData1Temp;
        exmem.readData2 = idex.readData2;
        exmem.Rt = idex.Rt;
        exmem.Rd = idex.Rd;
        exmem.signExtendImm = idex.signExtendImm;
        exmem.jumpReg = alu.jumpReg;

		uint32_t jumpAddress = exmem.instruction & 0b11111111111111111111111111; //Instruction [25-0]
		uint32_t PCoption = exmem.PC; //Chooses to jump to this or to PC+4, PCoption only used in branches and stores; doesnt jump yet, needs to jump at end of cycle
		if (exmem.control.jump == 1) {
			if (exmem.control.jumpLink == 1) {
				uint32_t temp = exmem.PC + 4; //PC + 8
				reg_file.access(0, 0, dummy1, dummy2, 31, exmem.control.reg_write, temp); //R31 = PC + 8
			}
			jumpAddress = jumpAddress << 2;
			uint32_t temp = exmem.PC & 0b11110000000000000000000000000000; //PC + 4 [31-28]
			jumpAddress += temp; //Jump Address [31=0]
			PCoption = jumpAddress; //next PC jump address
			exmem.PCsrc = true;
		}
		else if (exmem.control.branch == 1 && exmem.control.branchNotEqual == 0 && exmem.zeroFlag == 1) { //next PC Address MUX
			PCoption = exmem.PCbranch;
			exmem.PCsrc = true;
		}
		else if (exmem.control.branch == 1 && exmem.control.branchNotEqual == 1 && exmem.zeroFlag == 0) { //controls BNE MUX
			PCoption = exmem.PCbranch;
			exmem.PCsrc = true;
		}
		else if (exmem.jumpReg == true) { //controls Jump Register MUX
			PCoption = exmem.readData1;
			exmem.PCsrc = true;
		}
		else {
			exmem.PCsrc = false;
		}

        //Decode -> Process instruction
        control_t controlUnit; //initialize control signals for decoded instruction
        controlUnit.decode(ifid.opcode);
        uint32_t readData1;
        uint32_t readData2;
        reg_file.access(ifid.Rs, ifid.Rt, readData1, readData2, 0, 0, 0);
        if (ifid.Imm >> 15 == 1) {
            signExtend = ifid.Imm | 0b11111111111111110000000000000000;
        }
        else {
            signExtend = ifid.Imm;
        }
        //Stalling detection
		bool stallPipeline = false;
		if (exmem.control.mem_read == true) {
			if (exmem.Rt == ifid.Rs || (exmem.Rt == ifid.Rt && (controlUnit.ALU_src == 0 || controlUnit.mem_write == true))) {
				stallPipeline = true;
			}
		}
        
		
		if (stallPipeline) {
			//STALL, nothing is written to the idex
            idex.empty = true;
            idex.control = {.reg_dest = false, .jump = false, .branch = false, .mem_read = false, .mem_to_reg = false, .ALU_op = 3, .mem_write = false, .ALU_src = false, .reg_write = false, .branchNotEqual = false, .jumpLink = false, .loadUpperImm = false, .storeByte = false, .storeHalfWord = false, .loadByteU = false, .loadHalfWordU = false};
            idex.instruction = 0;
            idex.PC = 0;
            idex.readData1 = 0;
            idex.readData2 = 0;
			idex.Rs = 0;
            idex.Rt = 0;
            idex.Rd = 0;
            idex.opcode = 0;
            idex.Imm = 0;
            idex.signExtendImm = 0;
            idex.Shamt = 0;
            idex.Funct = 0;
        }
        else {
            //IDEX Pipeline -> Instruction writes into pipeline
            idex.empty = ifid.empty;
            idex.control = controlUnit;
            idex.instruction = ifid.instruction;
            idex.PC = ifid.PC;
            idex.readData1 = readData1;
            idex.readData2 = readData2;
            idex.Rs = ifid.Rs;
            idex.Rt = ifid.Rt;
            idex.Rd = ifid.Rd;
            idex.opcode = ifid.opcode;
            idex.Imm = ifid.Imm;
            idex.signExtendImm = signExtend;
            idex.Shamt = ifid.Shamt;
            idex.Funct = ifid.Funct;
        }

		//MEM->EX Forwarding
		if (memwb.control.mem_read == false && memwb.control.mem_write == false && memwb.control.branch == false && memwb.control.jump == false && memwb.empty == false && idex.empty == false) {
			if ((memwb.control.ALU_src == false && memwb.Rd == idex.Rs) || (memwb.control.ALU_src == true && memwb.Rt == idex.Rs)) {
				idex.readData1 = memwb.ALUresult;
			}
			if ((memwb.control.ALU_src == false && memwb.Rd == idex.Rt) || (memwb.control.ALU_src == true && memwb.Rt == idex.Rt)) {
				idex.readData2 = memwb.ALUresult;
			}
		}

		if (memwb.control.mem_read == true && idex.empty == false) {
			if (memwb.Rt == idex.Rs) {
				idex.readData1 = memwb.memReadData;
			}
			if (memwb.Rt == idex.Rt) {
				idex.readData2 = memwb.memReadData;
			}
		}

		//EX->EX Forwarding
		if (exmem.control.mem_read == false && exmem.control.mem_write == false && exmem.control.branch == false && exmem.control.jump == false && exmem.empty == false && idex.empty == false) {
			if ((exmem.control.ALU_src == false && exmem.Rd == idex.Rs) || (exmem.control.ALU_src == true && exmem.Rt == idex.Rs)) {
				idex.readData1 = exmem.ALUresult;
			}
			if ((exmem.control.ALU_src == false && exmem.Rd == idex.Rt) || (exmem.control.ALU_src == true && exmem.Rt == idex.Rt)) {
				idex.readData2 = exmem.ALUresult;
			}
		}

        //Fetch -> Retrieve instruction from PC
        uint32_t instruction;
        memory.access(reg_file.pc, instruction, 0, 1, 0);
        uint32_t opcode = instruction >> 26; //Instruction[31-26]
        uint32_t Rs = (instruction >> 21) & 0b11111; //Instruction [25-21]
        uint32_t Rt = (instruction >> 16) & 0b11111; //Instruction [20-16]
        uint32_t Rd = (instruction >> 11) & 0b11111; //Instruction [15-11]
        uint32_t Imm = instruction & 0b1111111111111111; //Instruction [15-0]
        uint32_t Shamt = (instruction >> 6) &0b11111; //Instruction [10-6]
        uint32_t Funct = instruction & 0b111111; //Instruction [5-0]
        uint32_t PC = reg_file.pc + 4; //save PC + 4 and propagate
        if (stallPipeline == false && exmem.PCsrc == false) {
            //IFID Pipeline -> Instruction writes into pipeline
            reg_file.pc += 4;
            ifid.empty = false;
            ifid.PC = PC;
            ifid.instruction = instruction;
            ifid.opcode = opcode;
            ifid.Rs = Rs;
            ifid.Rt = Rt;
            ifid.Rd = Rd;
            ifid.Imm = Imm;
            ifid.Shamt = Shamt;
            ifid.Funct = Funct;  
		}
		else if (exmem.PCsrc == true) { //Flushing, if it would have branched, set the ifid and idex pipelines to empty
			reg_file.pc = PCoption;

			ifid.empty = true;
			ifid.PC = 0;
			ifid.instruction = 0;
			ifid.opcode = 0;
			ifid.Rs = 0;
			ifid.Rt = 0;
			ifid.Rd = 0;
			ifid.Imm = 0;
			ifid.Shamt = 0;
			ifid.Funct = 0;

			idex.empty = true;
			idex.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex.instruction = 0;
			idex.PC = 0;
			idex.readData1 = 0;
			idex.readData2 = 0;
			idex.Rs = 0;
			idex.Rt = 0;
			idex.Rd = 0;
			idex.opcode = 0;
			idex.Imm = 0;
			idex.signExtendImm = 0;
			idex.Shamt = 0;
			idex.Funct = 0;
		}
		
		cout << "CYCLE" << num_cycles << "\n";
		reg_file.print(); // used for automated testing
		num_cycles++;

        //Update number of instructions committed
        num_instrs += committed_insts;
		if (endIt == true) {
			break;
		}
    }
    cout << "CPI = " << (double)num_cycles/(double)num_instrs << "\n";
} 

//Predict
bool predictBranchGHR(uint32_t nextInstruction, uint32_t GHR, int (&BHT)[256]) {
	uint32_t BHTIndex = (nextInstruction & 0b11111111) ^ GHR; //least significant 6-bits of program counter XOR'ed with 6-bit GHR
	int predict = BHT[BHTIndex];
	if (predict == 0 || predict == 1) { //00 or 01 (strongly NT or weakly NT)
		return false;
	}
	else {
		return true;
	}
}

//Update
void updateBHT_GHR(int (&BHT)[256], uint32_t &GHR, bool actual, uint32_t PC) {
	uint32_t predicted = (PC & 0b11111111) ^ GHR;
	if (actual == true) { //Taken
		BHT[predicted] = min(BHT[predicted]+1, 3);
		GHR = GHR >> 1;
		GHR += 128; //update most significant bit (3rd bit) to 1
	}
	else { //Not Taken
		BHT[predicted] = max(BHT[predicted]-1, 0);
		GHR = GHR >> 1; //updates most significant bit (3rd bit) to 0
	}
}

void speculative_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc) {
	// Initialize ALU
	ALU alu;
	// Initialize Pipeline registers
	IFID ifid = { .empty = true,.PC = 0,.instruction = 0,.opcode = 0,.Rs = 0,.Rt = 0,.Rd = 0,.Imm = 0,.Shamt = 0,.Funct = 0 };
	IDEX idex = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.PC = 0,.readData1 = 0,.readData2 = 0,.Rs = 0,.Rt = 0,.Rd = 0,.opcode = 0,.Imm = 0,.signExtendImm = 0,.Shamt = 0,.Funct = 0 };
	EXMEM exmem = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.PC = 0,.PCbranch = 0,.zeroFlag = 0,.ALUresult = 0,.readData1 = 0,.readData2 = 0,.Rt = 0,.Rd = 0,.regDestination = 0,.signExtendImm = 0,.jumpReg = false,.PCsrc = false };
	MEMWB memwb = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.Rt = 0,.Rd = 0,.memReadData = 0,.ALUresult = 0,.regDestination = 0,.PC = 0,.PCsrc = false,.jumpReg = false };
	uint32_t num_cycles = 0;
	uint32_t num_instrs = 0;

	uint32_t GHR = 0; //3-bit GHR

	int BHT[256]; //Initialize Branch History Table
	for (int i=0; i<256; i++) {
		BHT[i] = 1; //initalize to all "weakly NT"
		//00 - strongly NT
		//01 - weakly NT
		//10 - weakly T
		//11 - strongly T
	}

	while (true) {
		uint32_t committed_insts = 0;
		bool endIt = false;

		//Writeback -> Data writes into PC/Register file
		uint32_t dummy1 = 0;
		uint32_t dummy2 = 0;

		if (memwb.empty == true) {}
		else if (memwb.control.loadUpperImm == 1) { //Load Upper Immediate
			uint32_t temp = memwb.instruction << 16; //makes Imm most significant 16 bits
			reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, temp); //writes data back
		}
		else if (memwb.control.jumpLink == 0 && memwb.jumpReg == 0 && memwb.control.branch == 0 && memwb.control.branchNotEqual == 0) { //Prevents Jump and Link from working correctly
			if (memwb.control.mem_to_reg == 1) { //write back memory read result
				reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, memwb.memReadData); //RegDst determined in EX stage for Pipeline processor instead
			}
			else if (memwb.control.mem_to_reg == 0) { //write back ALU result
				reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, memwb.ALUresult); //RegDst determined in EX stage for Pipeline processor instead
			}
		}

		if (memwb.empty == false) {
			committed_insts = 1;
		}

		if (memwb.PC - 4 == end_pc) { //If the memwb's PC is the last instruction (memwb.PC carries PC+4, next instructions PC), end loop at the end of the cycle 
			endIt = true;
		}

		//Memory -> Read memory 
		uint32_t memReadResult = 0;
		uint32_t dummy3;

		if (exmem.empty == true) {}
		if (exmem.control.storeByte == 1) {
			uint32_t readData2Temp = exmem.readData2 & 0b11111111; //chops 24 bits off from read data 2
			memory.access(exmem.ALUresult, memReadResult, 0, true, 0); //M[address + displacement]

			if (exmem.ALUresult % 4 == 3) {
				memReadResult = memReadResult & 0b11111111111111111111111100000000; //M[address + displacement](31:8)
				memReadResult = memReadResult | readData2Temp;
			}
			else if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b11111111111111110000000011111111;
				readData2Temp = readData2Temp << 8;
				memReadResult = memReadResult | readData2Temp;
			}
			else if (exmem.ALUresult % 4 == 1) {
				memReadResult = memReadResult & 0b11111111000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult = memReadResult | readData2Temp;
			}
			else {
				memReadResult = memReadResult & 0b00000000111111111111111111111111;
				readData2Temp = readData2Temp << 24;
				memReadResult = memReadResult | readData2Temp;
			}
			memory.access(exmem.ALUresult, dummy3, memReadResult, 0, 1); //Store M[address + displacement](7:0) = Rt(7:0)
		}
		else if (exmem.control.storeHalfWord == 1) {
			uint32_t readData2Temp = exmem.readData2 & 0b1111111111111111; //chops 16 bits off from read data 2
			memory.access(exmem.ALUresult, memReadResult, 0, true, 0); //M[address + displacement]
			if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b11111111111111110000000000000000; //M[address + displacement](31:16)
				memReadResult = memReadResult | readData2Temp;
			}
			else {
				memReadResult = memReadResult & 0b00000000000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult = memReadResult | readData2Temp;
			}
			memory.access(exmem.ALUresult, dummy3, memReadResult, 0, 1); //Store M[address + displacement](15:0) = Rt(15:0)
		}
		else {
			//Memory access
			memory.access(exmem.ALUresult, memReadResult, exmem.readData2, exmem.control.mem_read, exmem.control.mem_write); //Load and Store
		}

		if (exmem.control.loadByteU == 1) {
			if (exmem.ALUresult % 4 == 3) {
				memReadResult = memReadResult & 0b11111111; //R[rt]=M[R[rs]+SignExtImm](7:0)
			}
			else if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b1111111100000000;
				memReadResult = memReadResult >> 8;
			}
			else if (exmem.ALUresult % 4 == 1) {
				memReadResult = memReadResult & 0b111111110000000000000000;
				memReadResult = memReadResult >> 16;
			}
			else {
				memReadResult = memReadResult & 0b11111111000000000000000000000000;
				memReadResult = memReadResult >> 24;
			}
		}
		else if (exmem.control.loadHalfWordU == 1) {
			if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b1111111111111111; //R[rt]=M[R[rs]+SignExtImm](15:0)
			}
			else {
				memReadResult = memReadResult & 0b11111111111111110000000000000000;
				memReadResult = memReadResult >> 16;
			}
		}

		//MEMWB Pipeline -> Memory writes into pipeline
		memwb.empty = exmem.empty;
		memwb.control = exmem.control;
		memwb.instruction = exmem.instruction;
		memwb.Rt = exmem.Rt;
		memwb.Rd = exmem.Rd;
		memwb.memReadData = memReadResult;
		memwb.ALUresult = exmem.ALUresult;
		memwb.jumpReg = exmem.jumpReg;
		memwb.regDestination = exmem.regDestination;
		memwb.PC = exmem.PC;
		memwb.PCsrc = exmem.PCsrc;

		//MEM->MEM Forwarding
		if (memwb.control.mem_read == true && idex.control.mem_write == true) { //checking LW then SW dependency
			idex.readData2 = memwb.memReadData; //SW rt = LW rt
		}

		//Execute -> ALU
		alu.generate_control_inputs(idex.control.ALU_op, idex.Funct, idex.opcode);
		uint32_t signExtend = idex.signExtendImm;
		if (alu.zeroExtend == true) { //Special cases: Andi and Ori
			signExtend = idex.Imm;
		}
		uint32_t readData1Temp = idex.readData1;
		if (alu.shift == true) { //Shifts
			readData1Temp = idex.Shamt;
		}
		uint32_t alu_result = 0;
		uint32_t zeroFlag = 0;

		if (idex.empty) {}
		else if (idex.control.ALU_src == 0) {
			alu_result = alu.execute(readData1Temp, idex.readData2, zeroFlag);
		}
		else if (idex.control.ALU_src == 1) {
			alu_result = alu.execute(readData1Temp, signExtend, zeroFlag);
		}

		if (idex.control.reg_dest == 0) {
			exmem.regDestination = idex.Rt;
		}
		else {
			exmem.regDestination = idex.Rd;
		}

		//EXMEM Pipeline -> ALU result writes into pipeline
		exmem.empty = idex.empty;
		exmem.control = idex.control;
		exmem.instruction = idex.instruction;
		exmem.PC = idex.PC;
		exmem.PCbranch = (idex.signExtendImm << 2) + idex.PC; //branch Address
		exmem.zeroFlag = zeroFlag;
		exmem.ALUresult = alu_result;
		exmem.readData1 = readData1Temp;
		exmem.readData2 = idex.readData2;
		exmem.Rt = idex.Rt;
		exmem.Rd = idex.Rd;
		exmem.signExtendImm = idex.signExtendImm;
		exmem.jumpReg = alu.jumpReg;
		exmem.branchPred = idex.branchPred;

		uint32_t jumpAddress = exmem.instruction & 0b11111111111111111111111111; //Instruction [25-0]
		uint32_t PCoption = exmem.PC; //Chooses to jump to this or to PC+4, PCoption only used in branches and stores; doesnt jump yet, needs to jump at end of cycle
		if (exmem.control.jump == 1) {
			if (exmem.control.jumpLink == 1) {
				uint32_t temp = exmem.PC + 4; //PC + 8
				reg_file.access(0, 0, dummy1, dummy2, 31, exmem.control.reg_write, temp); //R31 = PC + 8
			}
			jumpAddress = jumpAddress << 2;
			uint32_t temp = exmem.PC & 0b11110000000000000000000000000000; //PC + 4 [31-28]
			jumpAddress += temp; //Jump Address [31=0]
			PCoption = jumpAddress; //next PC jump address
			exmem.PCsrc = true;
		}
		else if (exmem.control.branch == 1 && exmem.control.branchNotEqual == 0) { //next PC Address MUX
			if (exmem.zeroFlag == 1) { //Taken
				updateBHT_GHR(BHT, GHR, true, exmem.PC-4);
				PCoption = exmem.PCbranch;
				exmem.PCsrc = true;
			}
			else if (exmem.zeroFlag == 0) { //Not Taken
				updateBHT_GHR(BHT, GHR, false, exmem.PC-4);
				PCoption = exmem.PC;
				exmem.PCsrc = false;
			}
		}
		else if (exmem.control.branch == 1 && exmem.control.branchNotEqual == 1) { //controls BNE MUX
			if (exmem.zeroFlag == 1) { //Not Taken
				updateBHT_GHR(BHT, GHR, false, exmem.PC-4);
				PCoption = exmem.PC;
				exmem.PCsrc = false;
			}
			else if (exmem.zeroFlag == 0) { //Taken
				updateBHT_GHR(BHT, GHR, true, exmem.PC-4);
				PCoption = exmem.PCbranch;
				exmem.PCsrc = true;
			}
		}
		else if (exmem.jumpReg == true) { //controls Jump Register MUX
			PCoption = exmem.readData1;
			exmem.PCsrc = true;
		}
		else {
			exmem.PCsrc = false;
		}

		//Decode -> Process instruction
		control_t controlUnit; //initialize control signals for decoded instruction
		controlUnit.decode(ifid.opcode);
		uint32_t readData1;
		uint32_t readData2;
		reg_file.access(ifid.Rs, ifid.Rt, readData1, readData2, 0, 0, 0);
		if (ifid.Imm >> 15 == 1) {
			signExtend = ifid.Imm | 0b11111111111111110000000000000000;
		}
		else {
			signExtend = ifid.Imm;
		}
		//Stalling detection
		bool stallPipeline = false;
		if (exmem.control.mem_read == true) {
			if (exmem.Rt == ifid.Rs || (exmem.Rt == ifid.Rt && (controlUnit.ALU_src == 0 || controlUnit.mem_write == true))) {
				stallPipeline = true;
			}
		}

		if (stallPipeline) {
			//STALL, nothing is written to the idex
			idex.empty = true;
			idex.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex.instruction = 0;
			idex.PC = 0;
			idex.readData1 = 0;
			idex.readData2 = 0;
			idex.Rs = 0;
			idex.Rt = 0;
			idex.Rd = 0;
			idex.opcode = 0;
			idex.Imm = 0;
			idex.signExtendImm = 0;
			idex.Shamt = 0;
			idex.Funct = 0;
			idex.branchPred = false;
		}
		else {
			//IDEX Pipeline -> Instruction writes into pipeline
			idex.empty = ifid.empty;
			idex.control = controlUnit;
			idex.instruction = ifid.instruction;
			idex.PC = ifid.PC;
			idex.readData1 = readData1;
			idex.readData2 = readData2;
			idex.Rs = ifid.Rs;
			idex.Rt = ifid.Rt;
			idex.Rd = ifid.Rd;
			idex.opcode = ifid.opcode;
			idex.Imm = ifid.Imm;
			idex.signExtendImm = signExtend;
			idex.Shamt = ifid.Shamt;
			idex.Funct = ifid.Funct;
			idex.branchPred = ifid.branchPred;
		}

		//MEM->EX Forwarding
		if (memwb.control.mem_read == false && memwb.control.mem_write == false && memwb.control.branch == false && memwb.control.jump == false && memwb.empty == false && idex.empty == false) {
			if ((memwb.control.ALU_src == false && memwb.Rd == idex.Rs) || (memwb.control.ALU_src == true && memwb.Rt == idex.Rs)) {
				idex.readData1 = memwb.ALUresult;
			}
			if ((memwb.control.ALU_src == false && memwb.Rd == idex.Rt) || (memwb.control.ALU_src == true && memwb.Rt == idex.Rt)) {
				idex.readData2 = memwb.ALUresult;
			}
		}

		if (memwb.control.mem_read == true && idex.empty == false) {
			if (memwb.Rt == idex.Rs) {
				idex.readData1 = memwb.memReadData;
			}
			if (memwb.Rt == idex.Rt) {
				idex.readData2 = memwb.memReadData;
			}
		}

		//EX->EX Forwarding
		if (exmem.control.mem_read == false && exmem.control.mem_write == false && exmem.control.branch == false && exmem.control.jump == false && exmem.empty == false && idex.empty == false) {
			if ((exmem.control.ALU_src == false && exmem.Rd == idex.Rs) || (exmem.control.ALU_src == true && exmem.Rt == idex.Rs)) {
				idex.readData1 = exmem.ALUresult;
			}
			if ((exmem.control.ALU_src == false && exmem.Rd == idex.Rt) || (exmem.control.ALU_src == true && exmem.Rt == idex.Rt)) {
				idex.readData2 = exmem.ALUresult;
			}
		}

		//Fetch -> Retrieve instruction from PC
		uint32_t instruction;
		memory.access(reg_file.pc, instruction, 0, 1, 0);
		uint32_t opcode = instruction >> 26; //Instruction[31-26]
		uint32_t Rs = (instruction >> 21) & 0b11111; //Instruction [25-21]
		uint32_t Rt = (instruction >> 16) & 0b11111; //Instruction [20-16]
		uint32_t Rd = (instruction >> 11) & 0b11111; //Instruction [15-11]
		uint32_t Imm = instruction & 0b1111111111111111; //Instruction [15-0]
		uint32_t signExtendEarly;
		uint32_t Shamt = (instruction >> 6) & 0b11111; //Instruction [10-6]
		uint32_t Funct = instruction & 0b111111; //Instruction [5-0]
		uint32_t PC = reg_file.pc + 4; //save PC + 4 and propagate
		if (Imm >> 15 == 1) {
			signExtendEarly = Imm | 0b11111111111111110000000000000000;
		}
		else {
			signExtendEarly = Imm;
		}

		bool branchPrediction = false;
		if (opcode == 4 || opcode == 5) { //BEQ or BNE
			branchPrediction = predictBranchGHR(PC-4, GHR, BHT);
			if (branchPrediction == true) { //predict "Taken"
				reg_file.pc += signExtendEarly << 2;
			}
			else {
			}
		}

		//IFID Pipeline -> Instruction writes into pipeline
		if (stallPipeline == false && exmem.PCsrc == exmem.branchPred) {
			reg_file.pc += 4; //When Actual==NT and Predicted==NT
			ifid.empty = false;
			ifid.PC = PC;
			ifid.instruction = instruction;
			ifid.opcode = opcode;
			ifid.Rs = Rs;
			ifid.Rt = Rt;
			ifid.Rd = Rd;
			ifid.Imm = Imm;
			ifid.Shamt = Shamt;
			ifid.Funct = Funct;
			ifid.branchPred = branchPrediction;
		}
		//else if (exmem.PCsrc == true) {}
		else if (stallPipeline == false && exmem.PCsrc != exmem.branchPred) { //Flushing, if it would have branched, set the ifid and idex pipelines to empty
		//Actual==T Predicted==NT (flush as usual) or Actual==NT Predicted==T (flush also)
			reg_file.pc = PCoption;

			ifid.empty = true;
			ifid.PC = 0;
			ifid.instruction = 0;
			ifid.opcode = 0;
			ifid.Rs = 0;
			ifid.Rt = 0;
			ifid.Rd = 0;
			ifid.Imm = 0;
			ifid.Shamt = 0;
			ifid.Funct = 0;
			ifid.branchPred = false;

			idex.empty = true;
			idex.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex.instruction = 0;
			idex.PC = 0;
			idex.readData1 = 0;
			idex.readData2 = 0;
			idex.Rs = 0;
			idex.Rt = 0;
			idex.Rd = 0;
			idex.opcode = 0;
			idex.Imm = 0;
			idex.signExtendImm = 0;
			idex.Shamt = 0;
			idex.Funct = 0;
			idex.branchPred = false;
		}

		//Update number of instructions committed
		num_instrs += committed_insts;
		if (endIt == true) {
			break;
		}

		cout << "CYCLE" << num_cycles << "\n";
		cout << "PC of next is: " << reg_file.pc << "\n";
		reg_file.print(); // used for automated testing
		num_cycles++;
	}
	cout << "CPI = " << (double)num_cycles / (double)num_instrs << "\n";
}

void io_superscalar_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc) {
	// Initialize ALU
	ALU alu;
	ALU alu2;
	// Initialize Pipeline registers
	IFID ifid = { .empty = true,.PC = 0,.instruction = 0,.opcode = 0,.Rs = 0,.Rt = 0,.Rd = 0,.Imm = 0,.Shamt = 0,.Funct = 0 };
	IDEX idex = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.PC = 0,.readData1 = 0,.readData2 = 0,.Rs = 0,.Rt = 0,.Rd = 0,.opcode = 0,.Imm = 0,.signExtendImm = 0,.Shamt = 0,.Funct = 0 };
	EXMEM exmem = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.PC = 0,.PCbranch = 0,.zeroFlag = 0,.ALUresult = 0,.readData1 = 0,.readData2 = 0,.Rt = 0,.Rd = 0,.regDestination = 0,.signExtendImm = 0,.jumpReg = false,.PCsrc = false };
	MEMWB memwb = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.Rt = 0,.Rd = 0,.memReadData = 0,.ALUresult = 0,.regDestination = 0,.PC = 0,.PCsrc = false,.jumpReg = false };
	
	IFID ifid2 = { .empty = true,.PC = 0,.instruction = 0,.opcode = 0,.Rs = 0,.Rt = 0,.Rd = 0,.Imm = 0,.Shamt = 0,.Funct = 0 };
	IDEX idex2 = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.PC = 0,.readData1 = 0,.readData2 = 0,.Rs = 0,.Rt = 0,.Rd = 0,.opcode = 0,.Imm = 0,.signExtendImm = 0,.Shamt = 0,.Funct = 0 };
	EXMEM exmem2 = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.PC = 0,.PCbranch = 0,.zeroFlag = 0,.ALUresult = 0,.readData1 = 0,.readData2 = 0,.Rt = 0,.Rd = 0,.regDestination = 0,.signExtendImm = 0,.jumpReg = false,.PCsrc = false };
	MEMWB memwb2 = { .empty = true,.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false },.instruction = 0,.Rt = 0,.Rd = 0,.memReadData = 0,.ALUresult = 0,.regDestination = 0,.PC = 0,.PCsrc = false,.jumpReg = false };
	
	uint32_t num_cycles = 0;
	uint32_t num_instrs = 0;

	uint32_t GHR = 0; //3-bit GHR

	int BHT[256]; //Initialize Branch History Table
	for (int i=0; i<256; i++) {
		BHT[i] = 1; //initalize to all "weakly NT"
	}

    while (true) {
		uint32_t committed_insts = 0;
		uint32_t committed_insts2 = 0;
		bool endIt = false;

		//Writeback -> Data writes into PC/Register file
		uint32_t dummy1 = 0;
		uint32_t dummy2 = 0;

		if (memwb.empty == true) {}
		else if (memwb.control.loadUpperImm == 1) { //Load Upper Immediate
			uint32_t temp = memwb.instruction << 16; //makes Imm most significant 16 bits
			reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, temp); //writes data back
		}
		else if (memwb.control.jumpLink == 0 && memwb.jumpReg == 0 && memwb.control.branch == 0 && memwb.control.branchNotEqual == 0) { //Prevents Jump and Link from working correctly
			if (memwb.control.mem_to_reg == 1) { //write back memory read result
				reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, memwb.memReadData); //RegDst determined in EX stage for Pipeline processor instead
			}
			else if (memwb.control.mem_to_reg == 0) { //write back ALU result
				reg_file.access(0, 0, dummy1, dummy2, memwb.regDestination, memwb.control.reg_write, memwb.ALUresult); //RegDst determined in EX stage for Pipeline processor instead
			}
		}

		if (memwb.empty == false) {
			committed_insts = 1;
		}

		if (memwb.PC - 4 == end_pc) { //If the memwb's PC is the last instruction (memwb.PC carries PC+4, next instructions PC), end loop at the end of the cycle 
			endIt = true;
		}

		//Writeback2
		if (memwb2.empty == true) {}
		else if (memwb2.control.loadUpperImm == 1) { //Load Upper Immediate
			uint32_t temp = memwb2.instruction << 16; //makes Imm most significant 16 bits
			reg_file.access(0, 0, dummy1, dummy2, memwb2.regDestination, memwb2.control.reg_write, temp); //writes data back
		}
		else if (memwb2.control.jumpLink == 0 && memwb2.jumpReg == 0 && memwb2.control.branch == 0 && memwb2.control.branchNotEqual == 0) { //Prevents Jump and Link from working correctly
			if (memwb2.control.mem_to_reg == 1) { //write back memory read result
				reg_file.access(0, 0, dummy1, dummy2, memwb2.regDestination, memwb2.control.reg_write, memwb2.memReadData); //RegDst determined in EX stage for Pipeline processor instead
			}
			else if (memwb2.control.mem_to_reg == 0) { //write back ALU result
				reg_file.access(0, 0, dummy1, dummy2, memwb2.regDestination, memwb2.control.reg_write, memwb2.ALUresult); //RegDst determined in EX stage for Pipeline processor instead
			}
		}

		if (memwb2.empty == false) {
			committed_insts2 = 1;
		}

		if (memwb2.PC - 4 == end_pc) { //If the memwb's PC is the last instruction (memwb.PC carries PC+4, next instructions PC), end loop at the end of the cycle 
			endIt = true;
		}

		//Memory -> Read memory 
		uint32_t memReadResult = 0;
		uint32_t dummy3;

		if (exmem.empty == true) {}
		if (exmem.control.storeByte == 1) {
			uint32_t readData2Temp = exmem.readData2 & 0b11111111; //chops 24 bits off from read data 2
			memory.access(exmem.ALUresult, memReadResult, 0, true, 0); //M[address + displacement]

			if (exmem.ALUresult % 4 == 3) {
				memReadResult = memReadResult & 0b11111111111111111111111100000000; //M[address + displacement](31:8)
				memReadResult = memReadResult | readData2Temp;
			}
			else if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b11111111111111110000000011111111;
				readData2Temp = readData2Temp << 8;
				memReadResult = memReadResult | readData2Temp;
			}
			else if (exmem.ALUresult % 4 == 1) {
				memReadResult = memReadResult & 0b11111111000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult = memReadResult | readData2Temp;
			}
			else {
				memReadResult = memReadResult & 0b00000000111111111111111111111111;
				readData2Temp = readData2Temp << 24;
				memReadResult = memReadResult | readData2Temp;
			}
			memory.access(exmem.ALUresult, dummy3, memReadResult, 0, 1); //Store M[address + displacement](7:0) = Rt(7:0)
		}
		else if (exmem.control.storeHalfWord == 1) {
			uint32_t readData2Temp = exmem.readData2 & 0b1111111111111111; //chops 16 bits off from read data 2
			memory.access(exmem.ALUresult, memReadResult, 0, true, 0); //M[address + displacement]
			if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b11111111111111110000000000000000; //M[address + displacement](31:16)
				memReadResult = memReadResult | readData2Temp;
			}
			else {
				memReadResult = memReadResult & 0b00000000000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult = memReadResult | readData2Temp;
			}
			memory.access(exmem.ALUresult, dummy3, memReadResult, 0, 1); //Store M[address + displacement](15:0) = Rt(15:0)
		}
		else {
			//Memory access
			memory.access(exmem.ALUresult, memReadResult, exmem.readData2, exmem.control.mem_read, exmem.control.mem_write); //Load and Store
		}

		if (exmem.control.loadByteU == 1) {
			if (exmem.ALUresult % 4 == 3) {
				memReadResult = memReadResult & 0b11111111; //R[rt]=M[R[rs]+SignExtImm](7:0)
			}
			else if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b1111111100000000;
				memReadResult = memReadResult >> 8;
			}
			else if (exmem.ALUresult % 4 == 1) {
				memReadResult = memReadResult & 0b111111110000000000000000;
				memReadResult = memReadResult >> 16;
			}
			else {
				memReadResult = memReadResult & 0b11111111000000000000000000000000;
				memReadResult = memReadResult >> 24;
			}
		}
		else if (exmem.control.loadHalfWordU == 1) {
			if (exmem.ALUresult % 4 == 2) {
				memReadResult = memReadResult & 0b1111111111111111; //R[rt]=M[R[rs]+SignExtImm](15:0)
			}
			else {
				memReadResult = memReadResult & 0b11111111111111110000000000000000;
				memReadResult = memReadResult >> 16;
			}
		}

		//Memory2
		uint32_t memReadResult2 = 0;

		if (exmem2.empty == true) {}
		if (exmem2.control.storeByte == 1) {
			uint32_t readData2Temp = exmem2.readData2 & 0b11111111; //chops 24 bits off from read data 2
			memory.access(exmem2.ALUresult, memReadResult2, 0, true, 0); //M[address + displacement]

			if (exmem2.ALUresult % 4 == 3) {
				memReadResult2 = memReadResult2 & 0b11111111111111111111111100000000; //M[address + displacement](31:8)
				memReadResult2 = memReadResult2 | readData2Temp;
			}
			else if (exmem2.ALUresult % 4 == 2) {
				memReadResult2 = memReadResult2 & 0b11111111111111110000000011111111;
				readData2Temp = readData2Temp << 8;
				memReadResult2 = memReadResult2 | readData2Temp;
			}
			else if (exmem2.ALUresult % 4 == 1) {
				memReadResult2 = memReadResult2 & 0b11111111000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult2 = memReadResult2 | readData2Temp;
			}
			else {
				memReadResult2 = memReadResult2 & 0b00000000111111111111111111111111;
				readData2Temp = readData2Temp << 24;
				memReadResult2 = memReadResult2 | readData2Temp;
			}
			memory.access(exmem2.ALUresult, dummy3, memReadResult2, 0, 1); //Store M[address + displacement](7:0) = Rt(7:0)
		}
		else if (exmem2.control.storeHalfWord == 1) {
			uint32_t readData2Temp = exmem2.readData2 & 0b1111111111111111; //chops 16 bits off from read data 2
			memory.access(exmem2.ALUresult, memReadResult2, 0, true, 0); //M[address + displacement]
			if (exmem2.ALUresult % 4 == 2) {
				memReadResult2 = memReadResult2 & 0b11111111111111110000000000000000; //M[address + displacement](31:16)
				memReadResult2 = memReadResult2 | readData2Temp;
			}
			else {
				memReadResult2 = memReadResult2 & 0b00000000000000001111111111111111;
				readData2Temp = readData2Temp << 16;
				memReadResult2 = memReadResult2 | readData2Temp;
			}
			memory.access(exmem2.ALUresult, dummy3, memReadResult2, 0, 1); //Store M[address + displacement](15:0) = Rt(15:0)
		}
		else {
			//Memory access
			memory.access(exmem2.ALUresult, memReadResult2, exmem2.readData2, exmem2.control.mem_read, exmem2.control.mem_write); //Load and Store
		}

		if (exmem2.control.loadByteU == 1) {
			if (exmem2.ALUresult % 4 == 3) {
				memReadResult2 = memReadResult2 & 0b11111111; //R[rt]=M[R[rs]+SignExtImm](7:0)
			}
			else if (exmem2.ALUresult % 4 == 2) {
				memReadResult2 = memReadResult2 & 0b1111111100000000;
				memReadResult2 = memReadResult2 >> 8;
			}
			else if (exmem2.ALUresult % 4 == 1) {
				memReadResult2 = memReadResult2 & 0b111111110000000000000000;
				memReadResult2 = memReadResult2 >> 16;
			}
			else {
				memReadResult2 = memReadResult2 & 0b11111111000000000000000000000000;
				memReadResult2 = memReadResult2 >> 24;
			}
		}
		else if (exmem2.control.loadHalfWordU == 1) {
			if (exmem2.ALUresult % 4 == 2) {
				memReadResult2 = memReadResult2 & 0b1111111111111111; //R[rt]=M[R[rs]+SignExtImm](15:0)
			}
			else {
				memReadResult2 = memReadResult2 & 0b11111111111111110000000000000000;
				memReadResult2 = memReadResult2 >> 16;
			}
		}

		//MEMWB Pipeline -> Memory writes into pipeline
		memwb.empty = exmem.empty;
		memwb.control = exmem.control;
		memwb.instruction = exmem.instruction;
		memwb.Rt = exmem.Rt;
		memwb.Rd = exmem.Rd;
		memwb.memReadData = memReadResult;
		memwb.ALUresult = exmem.ALUresult;
		memwb.jumpReg = exmem.jumpReg;
		memwb.regDestination = exmem.regDestination;
		memwb.PC = exmem.PC;
		memwb.PCsrc = exmem.PCsrc;

		//MEMWB Pipeline2
		memwb2.empty = exmem2.empty;
		memwb2.control = exmem2.control;
		memwb2.instruction = exmem2.instruction;
		memwb2.Rt = exmem2.Rt;
		memwb2.Rd = exmem2.Rd;
		memwb2.memReadData = memReadResult2;
		memwb2.ALUresult = exmem2.ALUresult;
		memwb2.jumpReg = exmem2.jumpReg;
		memwb2.regDestination = exmem2.regDestination;
		memwb2.PC = exmem2.PC;
		memwb2.PCsrc = exmem2.PCsrc;

		//MEM->MEM Forwarding (Superscalar update)
		//1-1
		if (memwb.control.mem_read == true && idex.control.mem_write == true) { //checking LW then SW dependency
			idex.readData2 = memwb.memReadData; //SW rt = LW rt
		}
		//1-2
		if (memwb.control.mem_read == true && idex2.control.mem_write == true) { //checking LW then SW dependency
			idex2.readData2 = memwb.memReadData; //SW rt = LW rt
		}
		//2-1
		if (memwb2.control.mem_read == true && idex.control.mem_write == true) { //checking LW then SW dependency
			idex.readData2 = memwb2.memReadData; //SW rt = LW rt
		}
		//2-2
		if (memwb2.control.mem_read == true && idex2.control.mem_write == true) { //checking LW then SW dependency
			idex2.readData2 = memwb2.memReadData; //SW rt = LW rt
		}

		//Execute -> ALU
		alu.generate_control_inputs(idex.control.ALU_op, idex.Funct, idex.opcode);
		uint32_t signExtend = idex.signExtendImm;
		if (alu.zeroExtend == true) { //Special cases: Andi and Ori
			signExtend = idex.Imm;
		}
		uint32_t readData1Temp = idex.readData1;
		if (alu.shift == true) { //Shifts
			readData1Temp = idex.Shamt;
		}
		uint32_t alu_result = 0;
		uint32_t zeroFlag = 0;

		if (idex.empty) {}
		else if (idex.control.ALU_src == 0) {
			alu_result = alu.execute(readData1Temp, idex.readData2, zeroFlag);
		}
		else if (idex.control.ALU_src == 1) {
			alu_result = alu.execute(readData1Temp, signExtend, zeroFlag);
		}

		if (idex.control.reg_dest == 0) {
			exmem.regDestination = idex.Rt;
		}
		else {
			exmem.regDestination = idex.Rd;
		}

		//Execute2
		alu2.generate_control_inputs(idex2.control.ALU_op, idex2.Funct, idex2.opcode);
		uint32_t signExtend2 = idex2.signExtendImm;
		if (alu2.zeroExtend == true) { //Special cases: Andi and Ori
			signExtend2 = idex2.Imm;
		}
		uint32_t readData1Temp2 = idex2.readData1;
		if (alu2.shift == true) { //Shifts
			readData1Temp2 = idex2.Shamt;
		}
		uint32_t alu_result2 = 0;
		uint32_t zeroFlag2 = 0;

		if (idex2.empty) {}
		else if (idex2.control.ALU_src == 0) {
			alu_result2 = alu2.execute(readData1Temp2, idex2.readData2, zeroFlag2);
		}
		else if (idex2.control.ALU_src == 1) {
			alu_result2 = alu2.execute(readData1Temp2, signExtend2, zeroFlag2);
		}

		if (idex2.control.reg_dest == 0) {
			exmem2.regDestination = idex2.Rt;
		}
		else {
			exmem2.regDestination = idex2.Rd;
		}

		//EXMEM Pipeline -> ALU result writes into pipeline
		exmem.empty = idex.empty;
		exmem.control = idex.control;
		exmem.instruction = idex.instruction;
		exmem.PC = idex.PC;
		exmem.PCbranch = (idex.signExtendImm << 2) + idex.PC; //branch Address
		exmem.zeroFlag = zeroFlag;
		exmem.ALUresult = alu_result;
		exmem.readData1 = readData1Temp;
		exmem.readData2 = idex.readData2;
		exmem.Rt = idex.Rt;
		exmem.Rd = idex.Rd;
		exmem.signExtendImm = idex.signExtendImm;
		exmem.jumpReg = alu.jumpReg;
		exmem.branchPred = idex.branchPred;

		//EXMEM Pipeline2
		exmem2.empty = idex2.empty;
		exmem2.control = idex2.control;
		exmem2.instruction = idex2.instruction;
		exmem2.PC = idex2.PC;
		exmem2.PCbranch = (idex2.signExtendImm << 2) + idex2.PC; //branch Address
		exmem2.zeroFlag = zeroFlag2;
		exmem2.ALUresult = alu_result2;
		exmem2.readData1 = readData1Temp2;
		exmem2.readData2 = idex2.readData2;
		exmem2.Rt = idex2.Rt;
		exmem2.Rd = idex2.Rd;
		exmem2.signExtendImm = idex2.signExtendImm;
		exmem2.jumpReg = alu2.jumpReg;
		exmem2.branchPred = idex2.branchPred;

		//Jump and Branch
		uint32_t jumpAddress = exmem.instruction & 0b11111111111111111111111111; //Instruction [25-0]
		uint32_t PCoption = exmem.PC; //Chooses to jump to this or to PC+4, PCoption only used in branches and stores; doesnt jump yet, needs to jump at end of cycle
		if (exmem.control.jump == 1) {
			if (exmem.control.jumpLink == 1) {
				uint32_t temp = exmem.PC + 4; //PC + 8
				reg_file.access(0, 0, dummy1, dummy2, 31, exmem.control.reg_write, temp); //R31 = PC + 8
			}
			jumpAddress = jumpAddress << 2;
			uint32_t temp = exmem.PC & 0b11110000000000000000000000000000; //PC + 4 [31-28]
			jumpAddress += temp; //Jump Address [31=0]
			PCoption = jumpAddress; //next PC jump address
			exmem.PCsrc = true;
		}
		else if (exmem.control.branch == 1 && exmem.control.branchNotEqual == 0) { //next PC Address MUX
			if (exmem.zeroFlag == 1) { //Taken
				updateBHT_GHR(BHT, GHR, true, exmem.PC-4);
				PCoption = exmem.PCbranch;
				exmem.PCsrc = true;
			}
			else if (exmem.zeroFlag == 0) { //Not Taken
				updateBHT_GHR(BHT, GHR, false, exmem.PC-4);
				PCoption = exmem.PC;
				exmem.PCsrc = false;
			}
		}
		else if (exmem.control.branch == 1 && exmem.control.branchNotEqual == 1) { //controls BNE MUX
			if (exmem.zeroFlag == 1) { //Not Taken
				updateBHT_GHR(BHT, GHR, false, exmem.PC-4);
				PCoption = exmem.PC;
				exmem.PCsrc = false;
			}
			else if (exmem.zeroFlag == 0) { //Taken
				updateBHT_GHR(BHT, GHR, true, exmem.PC-4);
				PCoption = exmem.PCbranch;
				exmem.PCsrc = true;
			}
		}
		else if (exmem.jumpReg == true) { //controls Jump Register MUX
			PCoption = exmem.readData1;
			exmem.PCsrc = true;
		}
		else {
			exmem.PCsrc = false;
		}

		//Jump and Branch 2
		uint32_t jumpAddress2 = exmem2.instruction & 0b11111111111111111111111111; //Instruction [25-0]
		uint32_t PCoption2 = exmem2.PC; //Chooses to jump to this or to PC+4, PCoption only used in branches and stores; doesnt jump yet, needs to jump at end of cycle
		if (exmem2.control.jump == 1) {
			if (exmem2.control.jumpLink == 1) {
				uint32_t temp = exmem2.PC + 4; //PC + 8
				reg_file.access(0, 0, dummy1, dummy2, 31, exmem2.control.reg_write, temp); //R31 = PC + 8
			}
			jumpAddress2 = jumpAddress2 << 2;
			uint32_t temp = exmem2.PC & 0b11110000000000000000000000000000; //PC + 4 [31-28]
			jumpAddress2 += temp; //Jump Address [31=0]
			PCoption2 = jumpAddress2; //next PC jump address
			exmem2.PCsrc = true;
		}
		else if (exmem2.control.branch == 1 && exmem2.control.branchNotEqual == 0) { //next PC Address MUX
			if (exmem2.zeroFlag == 1) { //Taken
				updateBHT_GHR(BHT, GHR, true, exmem2.PC-4);
				PCoption2 = exmem2.PCbranch;
				exmem2.PCsrc = true;
			}
			else if (exmem2.zeroFlag == 0) { //Not Taken
				updateBHT_GHR(BHT, GHR, false, exmem2.PC-4);
				PCoption2 = exmem2.PC;
				exmem2.PCsrc = false;
			}
		}
		else if (exmem2.control.branch == 1 && exmem2.control.branchNotEqual == 1) { //controls BNE MUX
			if (exmem2.zeroFlag == 1) { //Not Taken
				updateBHT_GHR(BHT, GHR, false, exmem2.PC-4);
				PCoption2 = exmem2.PC;
				exmem2.PCsrc = false;
			}
			else if (exmem2.zeroFlag == 0) { //Taken
				updateBHT_GHR(BHT, GHR, true, exmem2.PC-4);
				PCoption2 = exmem2.PCbranch;
				exmem2.PCsrc = true;
			}
		}
		else if (exmem2.jumpReg == true) { //controls Jump Register MUX
			PCoption2 = exmem2.readData1;
			exmem2.PCsrc = true;
		}
		else {
			exmem2.PCsrc = false;
		}

		//Decode -> Process instruction
		control_t controlUnit; //initialize control signals for decoded instruction
		controlUnit.decode(ifid.opcode);
		uint32_t readData1;
		uint32_t readData2;
		reg_file.access(ifid.Rs, ifid.Rt, readData1, readData2, 0, 0, 0);
		if (ifid.Imm >> 15 == 1) {
			signExtend = ifid.Imm | 0b11111111111111110000000000000000;
		}
		else {
			signExtend = ifid.Imm;
		}
		//Stalling detection
		bool stallPipeline = false;
		if (exmem.control.mem_read == true) {
			if (exmem.Rt == ifid.Rs || (exmem.Rt == ifid.Rt && (controlUnit.ALU_src == 0 || controlUnit.mem_write == true))) {
				stallPipeline = true;
			}
		}

		if (stallPipeline) {
			//STALL, nothing is written to the idex
			idex.empty = true;
			idex.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex.instruction = 0;
			idex.PC = 0;
			idex.readData1 = 0;
			idex.readData2 = 0;
			idex.Rs = 0;
			idex.Rt = 0;
			idex.Rd = 0;
			idex.opcode = 0;
			idex.Imm = 0;
			idex.signExtendImm = 0;
			idex.Shamt = 0;
			idex.Funct = 0;
			idex.branchPred = false;
		}
		else {
			//IDEX Pipeline -> Instruction writes into pipeline
			idex.empty = ifid.empty;
			idex.control = controlUnit;
			idex.instruction = ifid.instruction;
			idex.PC = ifid.PC;
			idex.readData1 = readData1;
			idex.readData2 = readData2;
			idex.Rs = ifid.Rs;
			idex.Rt = ifid.Rt;
			idex.Rd = ifid.Rd;
			idex.opcode = ifid.opcode;
			idex.Imm = ifid.Imm;
			idex.signExtendImm = signExtend;
			idex.Shamt = ifid.Shamt;
			idex.Funct = ifid.Funct;
			idex.branchPred = ifid.branchPred;
		}

		//Decode2
		control_t controlUnit2; //initialize control signals for decoded instruction
		controlUnit2.decode(ifid2.opcode);
		uint32_t readData11;
		uint32_t readData22;
		reg_file.access(ifid2.Rs, ifid2.Rt, readData11, readData22, 0, 0, 0);
		if (ifid2.Imm >> 15 == 1) {
			signExtend2 = ifid2.Imm | 0b11111111111111110000000000000000;
		}
		else {
			signExtend2 = ifid2.Imm;
		}
		//Stalling detection2 (Superscalar update)
		bool stallPipeline2 = false;
		if (exmem2.control.mem_read == true) {
			if (exmem2.Rt == ifid2.Rs || (exmem2.Rt == ifid2.Rt && (controlUnit2.ALU_src == 0 || controlUnit2.mem_write == true))) {
				stallPipeline2 = true;
			}
		}
		//ID->ID stall
		if ((controlUnit.ALU_src == false && ifid.Rd == ifid2.Rs) || (controlUnit.ALU_src == true && ifid.Rt == ifid2.Rs)) {
			stallPipeline2 = true;
		}
		if ((controlUnit.ALU_src == false && ifid.Rd == ifid2.Rt) || (controlUnit.ALU_src == true && ifid.Rt == ifid2.Rt)) {
			stallPipeline2 = true;
		}
		//ID->ID->ID stall
		if (exmem.control.mem_read == true) {
			if (exmem.Rt == ifid2.Rs || (exmem.Rt == ifid2.Rt && (controlUnit2.ALU_src == 0 || controlUnit2.mem_write == true))) {
				stallPipeline2 = true;
			}
		}

		if (stallPipeline2) {
			//STALL, nothing is written to the idex
			idex2.empty = true;
			idex2.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex2.instruction = 0;
			idex2.PC = 0;
			idex2.readData1 = 0;
			idex2.readData2 = 0;
			idex2.Rs = 0;
			idex2.Rt = 0;
			idex2.Rd = 0;
			idex2.opcode = 0;
			idex2.Imm = 0;
			idex2.signExtendImm = 0;
			idex2.Shamt = 0;
			idex2.Funct = 0;
			idex2.branchPred = false;
		}
		else {
			//IDEX Pipeline -> Instruction writes into pipeline
			idex2.empty = ifid2.empty;
			idex2.control = controlUnit2;
			idex2.instruction = ifid2.instruction;
			idex2.PC = ifid2.PC;
			idex2.readData1 = readData11;
			idex2.readData2 = readData22;
			idex2.Rs = ifid2.Rs;
			idex2.Rt = ifid2.Rt;
			idex2.Rd = ifid2.Rd;
			idex2.opcode = ifid2.opcode;
			idex2.Imm = ifid2.Imm;
			idex2.signExtendImm = signExtend2;
			idex2.Shamt = ifid2.Shamt;
			idex2.Funct = ifid2.Funct;
			idex2.branchPred = ifid2.branchPred;
		}

		//MEM->EX Forwarding (Superscalar update)
		//1-1
		if (memwb.control.mem_read == false && memwb.control.mem_write == false && memwb.control.branch == false && memwb.control.jump == false && memwb.empty == false && idex.empty == false) {
			if ((memwb.control.ALU_src == false && memwb.Rd == idex.Rs) || (memwb.control.ALU_src == true && memwb.Rt == idex.Rs)) {
				idex.readData1 = memwb.ALUresult;
			}
			if ((memwb.control.ALU_src == false && memwb.Rd == idex.Rt) || (memwb.control.ALU_src == true && memwb.Rt == idex.Rt)) {
				idex.readData2 = memwb.ALUresult;
			}
		}
		if (memwb.control.mem_read == true && idex.empty == false) {
			if (memwb.Rt == idex.Rs) {
				idex.readData1 = memwb.memReadData;
			}
			if (memwb.Rt == idex.Rt) {
				idex.readData2 = memwb.memReadData;
			}
		}
		//1-2
		if (memwb.control.mem_read == false && memwb.control.mem_write == false && memwb.control.branch == false && memwb.control.jump == false && memwb.empty == false && idex2.empty == false) {
			if ((memwb.control.ALU_src == false && memwb.Rd == idex2.Rs) || (memwb.control.ALU_src == true && memwb.Rt == idex2.Rs)) {
				idex2.readData1 = memwb.ALUresult;
			}
			if ((memwb.control.ALU_src == false && memwb.Rd == idex2.Rt) || (memwb.control.ALU_src == true && memwb.Rt == idex2.Rt)) {
				idex2.readData2 = memwb.ALUresult;
			}
		}
		if (memwb.control.mem_read == true && idex2.empty == false) {
			if (memwb.Rt == idex2.Rs) {
				idex2.readData1 = memwb.memReadData;
			}
			if (memwb.Rt == idex2.Rt) {
				idex2.readData2 = memwb.memReadData;
			}
		}
		//2-1
		if (memwb2.control.mem_read == false && memwb2.control.mem_write == false && memwb2.control.branch == false && memwb2.control.jump == false && memwb2.empty == false && idex.empty == false) {
			if ((memwb2.control.ALU_src == false && memwb2.Rd == idex.Rs) || (memwb2.control.ALU_src == true && memwb2.Rt == idex.Rs)) {
				idex.readData1 = memwb2.ALUresult;
			}
			if ((memwb2.control.ALU_src == false && memwb2.Rd == idex.Rt) || (memwb2.control.ALU_src == true && memwb2.Rt == idex.Rt)) {
				idex.readData2 = memwb2.ALUresult;
			}
		}
		if (memwb2.control.mem_read == true && idex.empty == false) {
			if (memwb2.Rt == idex.Rs) {
				idex.readData1 = memwb2.memReadData;
			}
			if (memwb2.Rt == idex.Rt) {
				idex.readData2 = memwb2.memReadData;
			}
		}
		//2-2
		if (memwb2.control.mem_read == false && memwb2.control.mem_write == false && memwb2.control.branch == false && memwb2.control.jump == false && memwb2.empty == false && idex2.empty == false) {
			if ((memwb2.control.ALU_src == false && memwb2.Rd == idex2.Rs) || (memwb2.control.ALU_src == true && memwb2.Rt == idex2.Rs)) {
				idex2.readData1 = memwb2.ALUresult;
			}
			if ((memwb2.control.ALU_src == false && memwb2.Rd == idex2.Rt) || (memwb2.control.ALU_src == true && memwb2.Rt == idex2.Rt)) {
				idex2.readData2 = memwb2.ALUresult;
			}
		}
		if (memwb2.control.mem_read == true && idex2.empty == false) {
			if (memwb2.Rt == idex2.Rs) {
				idex2.readData1 = memwb2.memReadData;
			}
			if (memwb2.Rt == idex2.Rt) {
				idex2.readData2 = memwb2.memReadData;
			}
		}

		//EX->EX Forwarding (Superscalar update)
		//1-1
		if (exmem.control.mem_read == false && exmem.control.mem_write == false && exmem.control.branch == false && exmem.control.jump == false && exmem.empty == false && idex.empty == false) {
			if ((exmem.control.ALU_src == false && exmem.Rd == idex.Rs) || (exmem.control.ALU_src == true && exmem.Rt == idex.Rs)) {
				idex.readData1 = exmem.ALUresult;
			}
			if ((exmem.control.ALU_src == false && exmem.Rd == idex.Rt) || (exmem.control.ALU_src == true && exmem.Rt == idex.Rt)) {
				idex.readData2 = exmem.ALUresult;
			}
		}
		//1-2
		if (exmem.control.mem_read == false && exmem.control.mem_write == false && exmem.control.branch == false && exmem.control.jump == false && exmem.empty == false && idex2.empty == false) {
			if ((exmem.control.ALU_src == false && exmem.Rd == idex2.Rs) || (exmem.control.ALU_src == true && exmem.Rt == idex2.Rs)) {
				idex2.readData1 = exmem.ALUresult;
			}
			if ((exmem.control.ALU_src == false && exmem.Rd == idex2.Rt) || (exmem.control.ALU_src == true && exmem.Rt == idex2.Rt)) {
				idex2.readData2 = exmem.ALUresult;
			}
		}
		//2-1
		if (exmem2.control.mem_read == false && exmem2.control.mem_write == false && exmem2.control.branch == false && exmem2.control.jump == false && exmem2.empty == false && idex.empty == false) {
			if ((exmem2.control.ALU_src == false && exmem2.Rd == idex.Rs) || (exmem2.control.ALU_src == true && exmem2.Rt == idex.Rs)) {
				idex.readData1 = exmem2.ALUresult;
			}
			if ((exmem2.control.ALU_src == false && exmem2.Rd == idex.Rt) || (exmem2.control.ALU_src == true && exmem2.Rt == idex.Rt)) {
				idex.readData2 = exmem2.ALUresult;
			}
		}
		//2-2
		if (exmem2.control.mem_read == false && exmem2.control.mem_write == false && exmem2.control.branch == false && exmem2.control.jump == false && exmem2.empty == false && idex2.empty == false) {
			if ((exmem2.control.ALU_src == false && exmem2.Rd == idex2.Rs) || (exmem2.control.ALU_src == true && exmem2.Rt == idex2.Rs)) {
				idex2.readData1 = exmem2.ALUresult;
			}
			if ((exmem2.control.ALU_src == false && exmem2.Rd == idex2.Rt) || (exmem2.control.ALU_src == true && exmem2.Rt == idex2.Rt)) {
				idex2.readData2 = exmem2.ALUresult;
			}
		}

		//Fetch -> Retrieve instruction from PC
		uint32_t instruction;
		memory.access(reg_file.pc, instruction, 0, 1, 0);
		uint32_t opcode = instruction >> 26; //Instruction[31-26]
		uint32_t Rs = (instruction >> 21) & 0b11111; //Instruction [25-21]
		uint32_t Rt = (instruction >> 16) & 0b11111; //Instruction [20-16]
		uint32_t Rd = (instruction >> 11) & 0b11111; //Instruction [15-11]
		uint32_t Imm = instruction & 0b1111111111111111; //Instruction [15-0]
		uint32_t signExtendEarly;
		uint32_t Shamt = (instruction >> 6) & 0b11111; //Instruction [10-6]
		uint32_t Funct = instruction & 0b111111; //Instruction [5-0]
		uint32_t PC = reg_file.pc + 4; //save PC + 4 and propagate      //reg_file.pc = 1000
		if (Imm >> 15 == 1) {
			signExtendEarly = Imm | 0b11111111111111110000000000000000;
		}
		else {
			signExtendEarly = Imm;
		}

		bool branchPrediction = false;
		if (opcode == 4 || opcode == 5) { //BEQ or BNE
			branchPrediction = predictBranchGHR(PC-4, GHR, BHT);
			if (branchPrediction == true) { //predict "Taken"
				reg_file.pc += signExtendEarly << 2;              //could be this : reg_file.pc = 1020
			}
			else {

			}
		}

		//Fetch2
		uint32_t instruction2;
		memory.access(reg_file.pc+4, instruction2, 0, 1, 0); //next simultaneous instruction (Superscalar update)
		uint32_t opcode2 = instruction2 >> 26; //Instruction[31-26]
		uint32_t Rs2 = (instruction2 >> 21) & 0b11111; //Instruction [25-21]
		uint32_t Rt2 = (instruction2 >> 16) & 0b11111; //Instruction [20-16]
		uint32_t Rd2 = (instruction2 >> 11) & 0b11111; //Instruction [15-11]
		uint32_t Imm2 = instruction2 & 0b1111111111111111; //Instruction [15-0]
		uint32_t signExtendEarly2;
		uint32_t Shamt2 = (instruction2 >> 6) & 0b11111; //Instruction [10-6]
		uint32_t Funct2 = instruction2 & 0b111111; //Instruction [5-0]
		uint32_t PC2 = reg_file.pc + 8; //save PC + 4 and propagate           //could be either 1000 or 1020
		if (Imm2 >> 15 == 1) {
			signExtendEarly2 = Imm2 | 0b11111111111111110000000000000000;
		}
		else {
			signExtendEarly2 = Imm2;
		}

		bool branchPrediction2 = false;
		if (opcode2 == 4 || opcode2 == 5) { //BEQ or BNE
			branchPrediction2 = predictBranchGHR(PC-4, GHR, BHT);
			if (branchPrediction2 == true) { //predict "Taken"
				reg_file.pc += signExtendEarly2 << 2;                     //could be this : reg_file.pc 1040
			}
			else {

			}
		}

		//IFID Pipeline -> Instruction writes into pipeline
		if (stallPipeline == false && exmem.PCsrc == exmem.branchPred) {
			reg_file.pc += 4; //When Actual==NT and Predicted==NT                // 1020+4     or     1040+4    or       1000+4
			ifid.empty = false;
			ifid.PC = PC;
			ifid.instruction = instruction;
			ifid.opcode = opcode;
			ifid.Rs = Rs;
			ifid.Rt = Rt;
			ifid.Rd = Rd;
			ifid.Imm = Imm;
			ifid.Shamt = Shamt;
			ifid.Funct = Funct;
			ifid.branchPred = branchPrediction;
		}
		else if (stallPipeline == false && exmem.PCsrc != exmem.branchPred) { //Flushing, if it would have branched, set the ifid and idex pipelines to empty
		//Actual==T Predicted==NT (flush as usual) or Actual==NT Predicted==T (flush also)
			reg_file.pc = PCoption;

			ifid.empty = true;
			ifid.PC = 0;
			ifid.instruction = 0;
			ifid.opcode = 0;
			ifid.Rs = 0;
			ifid.Rt = 0;
			ifid.Rd = 0;
			ifid.Imm = 0;
			ifid.Shamt = 0;
			ifid.Funct = 0;
			ifid.branchPred = false;

			idex.empty = true;
			idex.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex.instruction = 0;
			idex.PC = 0;
			idex.readData1 = 0;
			idex.readData2 = 0;
			idex.Rs = 0;
			idex.Rt = 0;
			idex.Rd = 0;
			idex.opcode = 0;
			idex.Imm = 0;
			idex.signExtendImm = 0;
			idex.Shamt = 0;
			idex.Funct = 0;
			idex.branchPred = false;

			ifid2.empty = true;
			ifid2.PC = 0;
			ifid2.instruction = 0;
			ifid2.opcode = 0;
			ifid2.Rs = 0;
			ifid2.Rt = 0;
			ifid2.Rd = 0;
			ifid2.Imm = 0;
			ifid2.Shamt = 0;
			ifid2.Funct = 0;
			ifid2.branchPred = false;

			idex2.empty = true;
			idex2.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex2.instruction = 0;
			idex2.PC = 0;
			idex2.readData1 = 0;
			idex2.readData2 = 0;
			idex2.Rs = 0;
			idex2.Rt = 0;
			idex2.Rd = 0;
			idex2.opcode = 0;
			idex2.Imm = 0;
			idex2.signExtendImm = 0;
			idex2.Shamt = 0;
			idex2.Funct = 0;
			idex2.branchPred = false;

			exmem2.empty = true;
			exmem2.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			exmem2.instruction = 0;
			exmem2.PC = 0;
			exmem2.PCbranch = 0;
			exmem2.zeroFlag = 0;
			exmem2.ALUresult = 0;
			exmem2.readData1 = 0;
			exmem2.readData2 = 0;
			exmem2.Rt = 0;
			exmem2.Rd = 0;
			exmem2.regDestination = 0;
			exmem2.signExtendImm = 0;
			exmem2.jumpReg = 0;
			exmem2.PCsrc = 0;
			exmem2.branchPred = false;
		}

		//IFID Pipeline2
		if (stallPipeline2 == false && exmem2.PCsrc == exmem2.branchPred) {
			reg_file.pc += 4; //When Actual==NT and Predicted==NT
			ifid2.empty = false;
			ifid2.PC = PC2;
			ifid2.instruction = instruction2;
			ifid2.opcode = opcode2;
			ifid2.Rs = Rs2;
			ifid2.Rt = Rt2;
			ifid2.Rd = Rd2;
			ifid2.Imm = Imm2;
			ifid2.Shamt = Shamt2;
			ifid2.Funct = Funct2;
			ifid2.branchPred = branchPrediction2;
		}
		else if (stallPipeline2 == false && exmem2.PCsrc != exmem2.branchPred) { //Flushing, if it would have branched, set the ifid and idex pipelines to empty
		//Actual==T Predicted==NT (flush as usual) or Actual==NT Predicted==T (flush also)
			reg_file.pc = PCoption2;

			ifid.empty = true;
			ifid.PC = 0;
			ifid.instruction = 0;
			ifid.opcode = 0;
			ifid.Rs = 0;
			ifid.Rt = 0;
			ifid.Rd = 0;
			ifid.Imm = 0;
			ifid.Shamt = 0;
			ifid.Funct = 0;
			ifid.branchPred = false;

			idex.empty = true;
			idex.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex.instruction = 0;
			idex.PC = 0;
			idex.readData1 = 0;
			idex.readData2 = 0;
			idex.Rs = 0;
			idex.Rt = 0;
			idex.Rd = 0;
			idex.opcode = 0;
			idex.Imm = 0;
			idex.signExtendImm = 0;
			idex.Shamt = 0;
			idex.Funct = 0;
			idex.branchPred = false;

			ifid2.empty = true;
			ifid2.PC = 0;
			ifid2.instruction = 0;
			ifid2.opcode = 0;
			ifid2.Rs = 0;
			ifid2.Rt = 0;
			ifid2.Rd = 0;
			ifid2.Imm = 0;
			ifid2.Shamt = 0;
			ifid2.Funct = 0;
			ifid2.branchPred = false;

			idex2.empty = true;
			idex2.control = { .reg_dest = false,.jump = false,.branch = false,.mem_read = false,.mem_to_reg = false,.ALU_op = 3,.mem_write = false,.ALU_src = false,.reg_write = false,.branchNotEqual = false,.jumpLink = false,.loadUpperImm = false,.storeByte = false,.storeHalfWord = false,.loadByteU = false,.loadHalfWordU = false };
			idex2.instruction = 0;
			idex2.PC = 0;
			idex2.readData1 = 0;
			idex2.readData2 = 0;
			idex2.Rs = 0;
			idex2.Rt = 0;
			idex2.Rd = 0;
			idex2.opcode = 0;
			idex2.Imm = 0;
			idex2.signExtendImm = 0;
			idex2.Shamt = 0;
			idex2.Funct = 0;
			idex2.branchPred = false;
		}

		//Update number of instructions committed
		num_instrs += committed_insts;
		num_instrs += committed_insts2;
		if (endIt == true) {
			break;
		}

		cout << "CYCLE" << num_cycles << "\n";
		cout << "PC of next is: " << reg_file.pc << "\n";
		reg_file.print(); // used for automated testing
		num_cycles++;
	}
    cout << "CPI = " << (double)num_cycles/(double)num_instrs << "\n";
}

void ooo_scalar_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc) {
    uint32_t num_cycles = 0;
    uint32_t num_instrs = 0; 

    /*while (true) {

        cout << "CYCLE" << num_cycles << "\n";

        reg_file.print(); // used for automated testing

        num_cycles++;

    }*/

    cout << "CPI = " << (double)num_cycles/(double)num_instrs << "\n";
}

void ooo_superscalar_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc) {
    uint32_t num_cycles = 0;
    uint32_t num_instrs = 0; 

    /*while (true) {

        cout << "CYCLE" << num_cycles << "\n";

        reg_file.print(); // used for automated testing

        num_cycles++;

    }*/

    cout << "CPI = " << (double)num_cycles/(double)num_instrs << "\n";
}