#include <stdio.h>
#include <stdint.h> // for uint32_t
#include <string.h>

#define MEMORY_SIZE 2048
#define REGISTER_COUNT 32

// Memory: 2048 x 32 bits
uint32_t memory[MEMORY_SIZE];

// Registers: R0 to R31
uint32_t registers[REGISTER_COUNT];

// Program Counter (PC)
uint32_t PC = 0;

// Initialize memory and registers
void initialize() {
    // Zero out memory
    for (int i = 0; i < MEMORY_SIZE; i++) {
        memory[i] = 0;
    }

    // Zero out registers
    for (int i = 0; i < REGISTER_COUNT; i++) {
        registers[i] = 0;
    }

    // R0 must always be hard-wired to 0
    registers[0] = 0;
}

int main() {
    initialize();

    printf("Initialization complete.\n");
    printf("Memory and registers are now all zeros.\n");
    return 0;
}
