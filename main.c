#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MEMORY_SIZE 2048
#define REGISTER_COUNT 32

uint32_t memory[MEMORY_SIZE];
uint32_t registers[REGISTER_COUNT];
uint32_t PC = 0;

// Map mnemonics to opcodes
int getOpcode(char* mnemonic) {
    if (strcmp(mnemonic, "ADD") == 0) return 0;
    if (strcmp(mnemonic, "SUB") == 0) return 1;
    if (strcmp(mnemonic, "MUL") == 0) return 2;
    if (strcmp(mnemonic, "MOVI") == 0) return 3;
    if (strcmp(mnemonic, "JEQ") == 0) return 4;
    if (strcmp(mnemonic, "AND") == 0) return 5;
    if (strcmp(mnemonic, "XORI") == 0) return 6;
    if (strcmp(mnemonic, "JMP") == 0) return 7;
    if (strcmp(mnemonic, "LSL") == 0) return 8;
    if (strcmp(mnemonic, "LSR") == 0) return 9;
    if (strcmp(mnemonic, "MOVR") == 0) return 10;
    if (strcmp(mnemonic, "MOVM") == 0) return 11;
    return -1; // Invalid
}

void initialize() {
    for (int i = 0; i < MEMORY_SIZE; i++) memory[i] = 0;
    for (int i = 0; i < REGISTER_COUNT; i++) registers[i] = 0;
    registers[0] = 0;
}

// Parses assembly file and fills memory[0-1023]
void loadInstructions(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        exit(1);
    }

    char line[100];
    int address = 0;

    while (fgets(line, sizeof(line), file)) {
        char mnemonic[10];
        char r1[10], r2[10], r3[10];
        int opcode, reg1, reg2, reg3, immediate, address_field;
        uint32_t instruction = 0;

        sscanf(line, "%s", mnemonic);
        opcode = getOpcode(mnemonic);

        if (opcode == -1) {
            printf("Unknown instruction: %s\n", mnemonic);
            continue;
        }

        instruction |= (opcode & 0xF) << 28; // Opcode: first 4 bits

        if (opcode <= 2 || opcode == 5 || opcode == 8 || opcode == 9) {
            // R-format
            sscanf(line, "%s %s %s %s", mnemonic, r1, r2, r3);
            reg1 = atoi(r1 + 1);
            reg2 = atoi(r2 + 1);
            if (opcode == 8 || opcode == 9) { // LSL/LSR use shift amount
                int shamt = atoi(r3);
                instruction |= (reg1 & 0x1F) << 23;
                instruction |= (reg2 & 0x1F) << 18;
                instruction |= (shamt & 0x1FFF);
            } else {
                reg3 = atoi(r3 + 1);
                instruction |= (reg1 & 0x1F) << 23;
                instruction |= (reg2 & 0x1F) << 18;
                instruction |= (reg3 & 0x1F) << 13;
            }
        } else if (opcode == 7) {
            // J-format
            sscanf(line, "%s %d", mnemonic, &address_field);
            instruction |= (address_field & 0x0FFFFFFF);
        } else {
            // I-format
            sscanf(line, "%s %s %s %d", mnemonic, r1, r2, &immediate);
            reg1 = atoi(r1 + 1);
            reg2 = atoi(r2 + 1);
            instruction |= (reg1 & 0x1F) << 23;
            instruction |= (reg2 & 0x1F) << 18;
            instruction |= (immediate & 0x3FFFF);
        }

        memory[address++] = instruction;
    }

    fclose(file);
}

int main() {
    initialize();
    loadInstructions("program.txt");

    printf("Instructions loaded into memory.\n");
    return 0;
}
