#ifndef MEMORY
#define MEMORY
#include <vector>
#include <cstdint>
#include <iostream>

class Memory {
    private:
        std::vector<uint32_t> mem;
    public:
        Memory() {
            mem.resize(65536, 0);
        }
	// address is the adress which needs to be read or written from
	// read_data the variable into which data is read, it is passed by reference
	// write_data is the data which is written into the memory address provided
	// mem_read specifies whether memory should be read or not
	// mem_write specifies whether memory whould be written to or not
        void access(uint32_t address, uint32_t &read_data, uint32_t write_data, bool mem_read, bool mem_write) {
			if (mem_read) {
				read_data = mem[address / 4];
			}
			if (mem_write) {
				mem[address / 4] = write_data;
			}
        }
        // given a starting address and number of words from that starting address
        // this function prints int values at the memory
        void print(uint32_t address, int num_words) {
            for(uint32_t i = address; i < address+num_words; ++i) {
                std::cout<< "MEM[" << i << "]: " << mem[i] << "\n";
            }
        }
};

#endif