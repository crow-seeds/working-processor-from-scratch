#ifndef CONTROL_CLASS
#define CONTROL_CLASS
#include <vector>
#include <cstdint>
#include <iostream>
using namespace std;

// Control signals for the processor
struct control_t {
    bool reg_dest;           // 0 if rt, 1 if rd
    bool jump;               // 1 if jummp
    bool branch;             // 1 if branch
    bool mem_read;           // 1 if memory needs to be read
    bool mem_to_reg;         // 1 if memory needs to written to reg
    unsigned ALU_op : 2;     // 10 for R-type, 00 for LW/SW/Add, 01 for BEQ/BNE, 11 for others
    bool mem_write;          // 1 if needs to be written to memory
    bool ALU_src;            // 0 if second operand is from reg_file, 1 if imm
    bool reg_write;          // 1 if need to write back to reg file
    bool branchNotEqual;
    bool jumpLink;
    bool loadUpperImm;
    bool storeByte;
    bool storeHalfWord;
    bool loadByteU;
    bool loadHalfWordU;

    void print() {      // Prints the generated contol signals
        cout << "REG_DEST: " << reg_dest << "\n";
        cout << "JUMP: " << jump << "\n";
        cout << "BRANCH: " << branch << "\n";
        cout << "MEM_READ: " << mem_read << "\n";
        cout << "MEM_TO_REG: " << mem_to_reg << "\n";
        cout << "ALU_OP: " << ALU_op << "\n";
        cout << "MEM_WRITE: " << mem_write << "\n";
        cout << "ALU_SRC: " << ALU_src << "\n";
        cout << "REG_WRITE: " << reg_write << "\n";
        cout << "BRANCH_NE: " << branchNotEqual << "\n";
        cout << "JUMP_LINK: " << jumpLink << "\n";
        cout << "LOAD_UPPER_IMM: " << loadUpperImm << "\n";
        cout << "STORE_BYTE: " << storeByte << "\n";
        cout << "STORE_HALFWORD: " << storeHalfWord << "\n";
        cout << "LOAD_BYTE_U: " << loadByteU << "\n";
        cout << "LOAD_HALFWORD_U: " << loadHalfWordU << "\n";
    }
    
    // Decode instructions into control signals
    void decode(uint32_t instruction) {
        uint32_t opcode = instruction; //Instruction[31-26]
        reg_dest = false;
        jump = false;
        branch = false;  
        mem_read = false;
        mem_to_reg = false;
        ALU_op = 3; //11 for others (never used anyways)
        mem_write = false; 
        ALU_src = false;
        reg_write = false;
        branchNotEqual = false;
        jumpLink = false;
        loadUpperImm = false;
        storeByte = false;
        storeHalfWord = false;
        loadByteU = false;
        loadHalfWordU = false;
        if (opcode == 0) { //R-Types
            reg_dest = true;
            ALU_op = 2; //10
            ALU_src = false;
            reg_write = true;
        }
        else { //I-Types
            ALU_src = true;
            reg_write = true;
        }
        if (opcode == 2) { //Jump
            jump = true;
            reg_write = false;
        }
        if (opcode == 4 || opcode == 5) { //Branch Equal, Branch Not Equal
            branch = true;
            ALU_op = 1;
            ALU_src = false;
            reg_write = false;
            if (opcode == 5) { //Branch Not Equal
                branchNotEqual = true;
            }
        }
        if (opcode == 35 || opcode == 36 || opcode == 37) { //Load Word, Load Byte Unsigned, Load Half Word Unsigned
            mem_read = true;
            mem_to_reg = true;
            ALU_op = 0;
            reg_write = true;
            if (opcode == 36) {
                loadByteU = true;
            }
            else if (opcode == 37) {
                loadHalfWordU = true;
            }
        }
        if (opcode == 40 || opcode == 41 || opcode == 43) { //Store Byte, Store Half Word, Store Word
            ALU_op = 0;
            mem_write = true;
            reg_write = false;
            if (opcode == 40) {
                storeByte = true;
            }
            else if (opcode == 41) {
                storeHalfWord = true;
            }
        }
        if (opcode == 3) { //Jump and Link
            jump = true;
            jumpLink = true;
            reg_write = true;
        }
        if (opcode == 15) { //Load Upper Immediate
            loadUpperImm = true;
        }
    }
};

#endif 