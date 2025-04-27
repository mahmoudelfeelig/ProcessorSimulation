#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MEMORY_SIZE 2048
#define REGISTER_COUNT 32

// Memory: 2048 rows of 32 bits
uint32_t memory[MEMORY_SIZE];

// Registers: R0 - R31
uint32_t registers[REGISTER_COUNT];

// Program Counter (PC)
uint32_t PC = 0;

// Struct for each stage in the pipeline
typedef struct {
    uint32_t instruction;
    int active;
    int opcode;
    int r1, r2, r3;
    int immediate;
    int address;
    uint32_t aluResult;
    uint32_t memoryData;
} PipelineStage;

// Pipeline stages
PipelineStage IF_stage, ID_stage, EX_stage, MEM_stage, WB_stage;

// Map mnemonic to opcode
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
    return -1;
}

// Initialize memory and registers
void initialize() {
    for (int i = 0; i < MEMORY_SIZE; i++) memory[i] = 0;
    for (int i = 0; i < REGISTER_COUNT; i++) registers[i] = 0;
    registers[0] = 0; // R0 is always 0
}

// Print non-zero memory contents
void printMemory() {
    printf("\n=== Memory Dump ===\n");
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i] != 0) {
            printf("Address [%d]: 0x%08X\n", i, memory[i]);
        }
    }
}

// Print all registers
void printRegisters() {
    printf("\n=== Registers Dump ===\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
        printf("R%d: 0x%08X\n", i, registers[i]);
    }
}

// Pretty print the pipeline stages
void printPipelineStages() {
    printf("+-------+-------------------------------+\n");
    printf("| Stage | Instruction                   |\n");
    printf("+-------+-------------------------------+\n");

    printf("|  IF   | ");
    IF_stage.active ? printf("0x%08X                 |\n", IF_stage.instruction) : printf("---                             |\n");

    printf("|  ID   | ");
    ID_stage.active ? printf("0x%08X                 |\n", ID_stage.instruction) : printf("---                             |\n");

    printf("|  EX   | ");
    EX_stage.active ? printf("0x%08X                 |\n", EX_stage.instruction) : printf("---                             |\n");

    printf("| MEM   | ");
    MEM_stage.active ? printf("0x%08X                 |\n", MEM_stage.instruction) : printf("---                             |\n");

    printf("|  WB   | ");
    WB_stage.active ? printf("0x%08X                 |\n", WB_stage.instruction) : printf("---                             |\n");

    printf("+-------+-------------------------------+\n");
}

// Load instructions from a file
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

        instruction |= (opcode & 0xF) << 28;

        if (opcode <= 2 || opcode == 5 || opcode == 8 || opcode == 9) {
            sscanf(line, "%s %s %s %s", mnemonic, r1, r2, r3);
            reg1 = atoi(r1 + 1);
            reg2 = atoi(r2 + 1);
            if (opcode == 8 || opcode == 9) {
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
            sscanf(line, "%s %d", mnemonic, &address_field);
            instruction |= (address_field & 0x0FFFFFFF);
        } else {
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

// Simulate the pipeline
void simulatePipeline() {
    int cycle = 1;
    int instructionsRemaining = 1;
    int fetchNext = 1;
    int fetchControl = 0;

    IF_stage.active = ID_stage.active = EX_stage.active = MEM_stage.active = WB_stage.active = 0;

    while (instructionsRemaining > 0) {
        printf("\nClock Cycle %d\n", cycle);
        printPipelineStages();

        // WB Stage
        if (WB_stage.active) {
            if (WB_stage.opcode <= 2 || WB_stage.opcode == 3 || WB_stage.opcode == 5 || WB_stage.opcode == 6 || WB_stage.opcode == 8 || WB_stage.opcode == 9) {
                registers[WB_stage.r1] = WB_stage.aluResult;
            } else if (WB_stage.opcode == 10) {
                registers[WB_stage.r1] = WB_stage.memoryData;
            }
            registers[0] = 0;
            WB_stage.active = 0;
        }

        // MEM Stage
        if (MEM_stage.active) {
            if (MEM_stage.opcode == 10) {
                MEM_stage.memoryData = memory[MEM_stage.aluResult];
            } else if (MEM_stage.opcode == 11) {
                memory[MEM_stage.aluResult] = registers[MEM_stage.r1];
            }
            WB_stage = MEM_stage;
            WB_stage.active = 1;
            MEM_stage.active = 0;
        }

        // EX Stage
        if (EX_stage.active) {
            switch (EX_stage.opcode) {
                case 0: EX_stage.aluResult = registers[EX_stage.r2] + registers[EX_stage.r3]; break;
                case 1: EX_stage.aluResult = registers[EX_stage.r2] - registers[EX_stage.r3]; break;
                case 2: EX_stage.aluResult = registers[EX_stage.r2] * registers[EX_stage.r3]; break;
                case 3: EX_stage.aluResult = EX_stage.immediate; break;
                case 4: if (registers[EX_stage.r1] == registers[EX_stage.r2]) PC += 1 + EX_stage.immediate; break;
                case 5: EX_stage.aluResult = registers[EX_stage.r2] & registers[EX_stage.r3]; break;
                case 6: EX_stage.aluResult = registers[EX_stage.r2] ^ EX_stage.immediate; break;
                case 7: PC = (PC & 0xF0000000) | EX_stage.address; break;
                case 8: EX_stage.aluResult = registers[EX_stage.r2] << EX_stage.r3; break;
                case 9: EX_stage.aluResult = registers[EX_stage.r2] >> EX_stage.r3; break;
                case 10: case 11: EX_stage.aluResult = registers[EX_stage.r2] + EX_stage.immediate; break;
            }
            MEM_stage = EX_stage;
            MEM_stage.active = 1;
            EX_stage.active = 0;
        }

        // ID Stage
        if (ID_stage.active) {
            uint32_t instr = ID_stage.instruction;
            ID_stage.opcode = (instr >> 28) & 0xF;

            if (ID_stage.opcode <= 2 || ID_stage.opcode == 5 || ID_stage.opcode == 8 || ID_stage.opcode == 9) {
                ID_stage.r1 = (instr >> 23) & 0x1F;
                ID_stage.r2 = (instr >> 18) & 0x1F;
                ID_stage.r3 = (instr >> 13) & 0x1F;
            } else if (ID_stage.opcode == 7) {
                ID_stage.address = instr & 0x0FFFFFFF;
            } else {
                ID_stage.r1 = (instr >> 23) & 0x1F;
                ID_stage.r2 = (instr >> 18) & 0x1F;
                ID_stage.immediate = instr & 0x3FFFF;
            }
            EX_stage = ID_stage;
            EX_stage.active = 1;
            ID_stage.active = 0;
        }

        // IF Stage
        if (fetchNext && PC < 1024) {
            IF_stage.instruction = memory[PC++];
            IF_stage.active = 1;
            fetchNext = 0;
        }

        if (IF_stage.active) {
            ID_stage = IF_stage;
            ID_stage.active = 1;
            IF_stage.active = 0;
        }

        cycle++;
        fetchControl++;
        if (fetchControl == 2) {
            fetchNext = 1;
            fetchControl = 0;
        }

        if (!IF_stage.active && !ID_stage.active && !EX_stage.active && !MEM_stage.active && !WB_stage.active && PC >= 1024) {
            instructionsRemaining = 0;
        }
    }
}

int main() {
    initialize();
    loadInstructions("program.txt");

    printf("\n=== Starting Simulation ===\n");
    simulatePipeline();

    printf("\n=== Simulation Complete ===\n");
    printRegisters();
    printMemory();

    return 0;
}
