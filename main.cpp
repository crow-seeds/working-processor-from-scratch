#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <elf.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <getopt.h>
#include "memory.h"
#include "reg_file.h"

using namespace std;

extern void single_cycle_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);
extern void pipelined_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);
extern void speculative_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);
extern void io_superscalar_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);
extern void ooo_scalar_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);
extern void ooo_superscalar_main_loop(Registers &reg_file, Memory &memory, uint32_t end_pc);

/* Load Binary. */
uint32_t load(char *bmk, Memory &memory)
{
  Elf32_Ehdr ehdr;
  Elf32_Shdr shdr;

  /* Open binary executable. */
  FILE *binary, *binary_copy;
  binary = fopen(bmk, "r");
  if (!binary) {
      cout << "Failed to open executable binary: " << string(optarg) << "\n";
      return 0;
  }

  /* Read and verify executable header. */
  int num_read = fread(&ehdr, 1, sizeof(ehdr), binary);
  if ((num_read != sizeof(ehdr)) || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7)) {
     cout << "Error in ELF header\n";
     return 0;
  }

  /* Read section headers. */
  fseek(binary, ehdr.e_shoff, SEEK_SET);
  for (int i = 0; i < ehdr.e_shnum; i++) {
      num_read = fread(&shdr, sizeof(shdr), 1, binary);
      if (num_read != 1) {
          cout << "Error in section header: " << num_read << " shdr=" << shdr.sh_addr << "\n";
          break;
      }
      uint32_t word, dummy_word;
      if ((shdr.sh_flags & SHF_EXECINSTR) != 0 && shdr.sh_addr == 0) { /* Text section -- we hardcoded this to zero during compilation. */
          binary_copy = fopen(bmk, "r");
          fseek(binary_copy, shdr.sh_offset, SEEK_SET);
          for (int j = 0; j < shdr.sh_size; j += 4) {
              num_read = fread(&word, 1, 4, binary_copy);
              if (num_read != 4) {
                  cout << "Could not populate memory from section: " << shdr.sh_addr <<
                          ": bytes read=" << j + num_read << ", section header size=" << shdr.sh_size << "\n";
                  return 0;
              }
              memory.access((uint32_t)shdr.sh_addr+j, dummy_word, word, false, true);
          }
          fclose(binary_copy);
          return shdr.sh_size;
      }
  }

  return 0;
}

void print_help()
{
    cout << "Required Options.\n" 
            "--bmk <path-to-executable>           Path to the benchmark executable binary.\n"
            "--processor <processor-type>         Type of the processor being simulated.  Can take any of the following values: \n"
            "                                         single-cycle: MIPS Single-Cycle Processor\n"
            "                                         pipelined: The 5-stage MIPS Pipeline\n"
            "                                         speculative: The 5-stage MIPS Pipeline with Branch Prediction\n"
            "                                         io-superscalar: A dual-issue inorder MIPS processor\n"
            "                                         out-of-order: A scalar out-of-order MIPS processor\n"
            "                                         ooo-superscalar: A dual-issue out-of-order MIPS processor\n"
            "                                     Defaults to single-cycle\n"
            "Optional:\n"
            "--help                               Print this help message\n";
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
      {"bmk", required_argument, 0, 'b'},
      {"processor", required_argument, 0, 'p'},
      {"help", no_argument, 0, 'h'}
    };
    int option_index = 0;
    bool initialized = false;
    FILE *binary;
    string processor_type;

    // Initialize memory
    Memory memory;

    // Initialize reg_file
    Registers reg_file;
    reg_file.pc = 0;
    uint32_t end_pc;

    while (true) {
      char c = getopt_long(argc, argv, "b:p:h", long_options, &option_index);
      if (c == -1) {
          if (!initialized) {
              print_help();
              exit(0);
          }
          break;
      }
      initialized = 1;
      switch (c) {
          default :
          case 'h':
              print_help();
              exit(0);
          case 'b':
              end_pc = load(optarg, memory);
              break;
          case 'p':
              processor_type = string(optarg);
              if (processor_type == "single-cycle") {
                  single_cycle_main_loop(reg_file, memory, end_pc);
              } else if (processor_type == "pipelined") {
                  pipelined_main_loop(reg_file, memory, end_pc);
              } else if (processor_type == "speculative") {
                  speculative_main_loop(reg_file, memory, end_pc);
              } else if (processor_type == "io-superscalar") {
                  io_superscalar_main_loop(reg_file, memory, end_pc);
              } else if (processor_type == "out-of-order") {
                  ooo_scalar_main_loop(reg_file, memory, end_pc);
              } else if (processor_type == "ooo-superscalar") {
                  ooo_superscalar_main_loop(reg_file, memory, end_pc);
              }
              break;
      }
    }
}