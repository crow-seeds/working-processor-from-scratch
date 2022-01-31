#ifndef ALU_CLASS
#define ALU_CLASS
#include <vector>
#include <cstdint>
#include <iostream>
class ALU {
    private:
        int ALU_control_inputs;
        bool unSigned;
    public:
        bool jumpReg;
        bool shift;
        bool zeroExtend;
        // Generate the control inputs for the ALU
        void generate_control_inputs(int ALU_op, int funct, int opcode) {
            unSigned = false;
            jumpReg = false;
            shift = false;
            zeroExtend = false;
			ALU_control_inputs = 0;

            if (ALU_op == 0) { //ALU_OP = 00 for LW/SW
                ALU_control_inputs = 0; //0000 for ADD 
            }
            if (ALU_op == 1) { //ALU_OP = 01 for BEQ/BNE
                ALU_control_inputs = 1; //0001 for SUBTRACT
            }
            if (ALU_op == 2) { //10 for R-type
                if (funct == 32) {
                    ALU_control_inputs = 0; //0000 for ADD
                }
                else if (funct == 33) {
                    ALU_control_inputs = 0; //0000 for ADD
                    unSigned = true; //for ADD Unsigned
                }
                else if (funct == 36) {
                    ALU_control_inputs = 2; //0010 for AND
                }
                else if (funct == 8) {
                    jumpReg = true; //controls JumpReg Mux
                }
                else if (funct == 39) {
                    ALU_control_inputs = 3; //0011 for NOR
                }
                else if (funct == 37) {
                    ALU_control_inputs = 4; //0100 for OR
                }
                else if (funct == 42) {
                    ALU_control_inputs = 5; //0101 for Set Less Than
                }
                else if (funct == 43) {
                    ALU_control_inputs = 5; //0101 for Set Less Than
                    unSigned = true; //for Set Less Than Unsigned
                }
                else if (funct == 0) {
                    ALU_control_inputs = 6; //0110 for Shift Left Logical
                    shift = true;
                }
                else if (funct == 2) {
                    ALU_control_inputs = 7; //0111 for Shift Right Logical
                    shift = true;
                }
                else if (funct == 34) {
                    ALU_control_inputs = 1; //0001 for Subtract
                }
                else if (funct == 35) {
                    ALU_control_inputs = 1; //0001 for Subtract
                    unSigned = true;
                }
            }
            if (opcode == 8) {
                ALU_control_inputs = 0; //0000 for ADD Immediate
            }
            else if (opcode == 9) {
                ALU_control_inputs = 0; //0000 for ADD Immediate Unsigned
                unSigned = true;
            }
            else if (opcode == 12) {
                ALU_control_inputs = 2; //0010 for AND Immediate
                zeroExtend = true;
            }
            else if (opcode == 13) {
                ALU_control_inputs = 4; //0100 for OR Immediate
                zeroExtend = true;
            }
            else if (opcode == 10) {
                ALU_control_inputs = 5; //0101 for Set Less Than Immediate
            }
            else if (opcode == 11) {
                ALU_control_inputs = 5; //0101 for Set Less Than Immediate Unsigned
                unSigned = true;
            }
        }
        
        // Execute ALU operations, generate result, and set the zero control signal if necessary
        uint32_t execute(uint32_t operand_1, uint32_t operand_2, uint32_t &ALU_zero) {
            if (ALU_control_inputs == 0) {
                return operand_1 + operand_2;
            }
            else if (ALU_control_inputs == 1) {
                if (operand_1 == operand_2) {
                    ALU_zero = 1;
                }
                return operand_1 - operand_2;
            }
            else if (ALU_control_inputs == 2) {
                return operand_1 & operand_2;
            }
            else if (ALU_control_inputs == 3) {
                return ~(operand_1 | operand_2);
            }
            else if (ALU_control_inputs == 4) {
                return operand_1 | operand_2;
            }
            else if (ALU_control_inputs == 5) {
                if (operand_1 < operand_2) {
                    return 1;
                }
                return 0;
            }
            else if (ALU_control_inputs == 6) {
                return operand_2 << operand_1; //operand_1 is shamt
            }
            else if(ALU_control_inputs == 7) {
                return operand_2 >> operand_1; //operand_1 is shamt
			}
			return 0;
        }   
};

#endif