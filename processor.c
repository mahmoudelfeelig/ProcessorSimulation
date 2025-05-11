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
// Instruction memory count
int instruction_count = 0;

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
    if (strcmp(mnemonic, "ADD") == 0)   return 0;
    if (strcmp(mnemonic, "SUB") == 0)   return 1;
    if (strcmp(mnemonic, "MUL") == 0)   return 2;
    if (strcmp(mnemonic, "MOVI") == 0)  return 3;
    if (strcmp(mnemonic, "JEQ") == 0)   return 4;
    if (strcmp(mnemonic, "AND") == 0)   return 5;
    if (strcmp(mnemonic, "XORI") == 0)  return 6;
    if (strcmp(mnemonic, "JMP") == 0)   return 7;
    if (strcmp(mnemonic, "LSL") == 0)   return 8;
    if (strcmp(mnemonic, "LSR") == 0)   return 9;
    if (strcmp(mnemonic, "MOVR") == 0)  return 10;
    if (strcmp(mnemonic, "MOVM") == 0)  return 11;
    return -1;
}

// Initialize memory and registers
void initialize() {
    for (int i = 0; i < MEMORY_SIZE; i++) memory[i] = 0;
    for (int i = 0; i < REGISTER_COUNT; i++) registers[i] = 0;
    registers[0] = 0; // R0 is always 0
}

// Print all registers
void printRegisters() {
    printf("\n=== Registers Dump ===\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
        printf("R%d: 0x%08X\n", i, registers[i]);
    }
    printf("PC: %u\n", PC);
}

// Pretty print the pipeline stages (fixed-width columns)
void printPipelineStages() {
    printf("+-------+--------------------------------+\n");
    printf("| Stage | Instruction                    |\n");
    printf("+-------+--------------------------------+\n");

    printf("|  IF   | ");
    if (IF_stage.active)
        printf("0x%08X                 |\n", IF_stage.instruction);
    else
        printf("---                         |\n");

    printf("|  ID   | ");
    if (ID_stage.active)
        printf("0x%08X                 |\n", ID_stage.instruction);
    else
        printf("---                         |\n");

    printf("|  EX   | ");
    if (EX_stage.active)
        printf("0x%08X                 |\n", EX_stage.instruction);
    else
        printf("---                         |\n");

    printf("|  MEM  | ");
    if (MEM_stage.active)
        printf("0x%08X                 |\n", MEM_stage.instruction);
    else
        printf("---                         |\n");

    printf("|  WB   | ");
    if (WB_stage.active)
        printf("0x%08X                 |\n", WB_stage.instruction);
    else
        printf("---                         |\n");

    printf("+-------+--------------------------------+\n");
}

// Print full instruction memory and only the non-zero data memory
void printMemory() {
    printf("\n=== Instruction Memory Dump ===\n");
    for (int i = 0; i < instruction_count; i++) {
        printf("Addr[%4d]: 0x%08X\n", i, memory[i]);
    }

    printf("\n=== Data Memory Dump (non-zero entries) ===\n");
    int found = 0;
    for (int i = instruction_count; i < MEMORY_SIZE; i++) {
        if (memory[i] != 0) {
            printf("Addr[%4d]: 0x%08X\n", i, memory[i]);
            found = 1;
        }
    }
    if (!found) {
        printf("  (all data memory is 0, as expected)\n");
    }
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
        int opcode, reg1, reg2, reg3, immediate, address_field;
        uint32_t instruction = 0;

        sscanf(line, "%s", mnemonic);
        opcode = getOpcode(mnemonic);
        if (opcode == -1) {
            printf("Unknown instruction: %s\n", mnemonic);
            continue;
        }

        // Build the 32-bit instruction
        instruction |= (opcode & 0xF) << 28;
        if (opcode <= 2 || opcode == 5 || opcode == 8 || opcode == 9) {
            // R-type or shift
            sscanf(line, "%*s R%d R%d R%d", &reg1, &reg2, &reg3);
            instruction |= (reg1 & 0x1F) << 23;
            instruction |= (reg2 & 0x1F) << 18;
            if (opcode == 8 || opcode == 9) {
                instruction |= (reg3 & 0x1FFF);
            } else {
                instruction |= (reg3 & 0x1F) << 13;
            }
        } else if (opcode == 7) {
            // Unconditional jump
            sscanf(line, "%*s %d", &address_field);
            instruction |= (address_field & 0x0FFFFFFF);
        } else {
            // I-type
            sscanf(line, "%*s R%d R%d %d", &reg1, &reg2, &immediate);
            instruction |= (reg1 & 0x1F) << 23;
            instruction |= (reg2 & 0x1F) << 18;
            instruction |= (immediate & 0x3FFFF);
        }

        memory[address++] = instruction;
    }
    fclose(file);
    instruction_count = address;
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

        // Print stage inputs
        printf("Stage Inputs:\n");
        if (IF_stage.active)  printf("  IF: instruction=0x%08X\n", IF_stage.instruction);
        if (ID_stage.active)  printf("  ID: instruction=0x%08X, opcode=%d\n",
                                   ID_stage.instruction, ID_stage.opcode);
        if (EX_stage.active)  printf("  EX: opcode=%d, r1=R%d, r2=R%d, r3=R%d, imm=%d\n",
                                   EX_stage.opcode, EX_stage.r1, EX_stage.r2,
                                   EX_stage.r3, EX_stage.immediate);
        if (MEM_stage.active) printf("  MEM: opcode=%d, aluResult=0x%08X\n",
                                   MEM_stage.opcode, MEM_stage.aluResult);
        if (WB_stage.active)  printf("  WB: opcode=%d, aluResult=0x%08X, memoryData=0x%08X\n",
                                   WB_stage.opcode, WB_stage.aluResult, WB_stage.memoryData);

        // Write-Back Stage
        if (WB_stage.active) {
            if (WB_stage.opcode <= 2 || WB_stage.opcode == 3 || WB_stage.opcode == 5 ||
                WB_stage.opcode == 6 || WB_stage.opcode == 8 || WB_stage.opcode == 9) {
                if (WB_stage.r1 != 0) {
                    registers[WB_stage.r1] = WB_stage.aluResult;
                    printf("WB: Register R%u updated to 0x%08X\n",
                           WB_stage.r1, WB_stage.aluResult);
                } else {
                    printf("WB: Write to R0 ignored\n");
                }
            } else if (WB_stage.opcode == 10) {
                if (WB_stage.r1 != 0) {
                    registers[WB_stage.r1] = WB_stage.memoryData;
                    printf("WB: Register R%u loaded from memory with value 0x%08X\n",
                           WB_stage.r1, WB_stage.memoryData);
                } else {
                    printf("WB: Write to R0 ignored\n");
                }
            }
            WB_stage.active = 0;
        }

        // Memory Stage
        if (MEM_stage.active) {
            if (MEM_stage.opcode == 10) {
                MEM_stage.memoryData = memory[MEM_stage.aluResult];
            } else if (MEM_stage.opcode == 11) {
                memory[MEM_stage.aluResult] = registers[MEM_stage.r1];
                printf("MEM: Memory[%u] updated to 0x%08X\n",
                       MEM_stage.aluResult, registers[MEM_stage.r1]);
            }
            WB_stage = MEM_stage;
            WB_stage.active = 1;
            MEM_stage.active = 0;
        }

        // Execute Stage (2-cycle execute simulated via fetchControl delays)
        if (EX_stage.active) {
            int branch_taken = 0;
            switch (EX_stage.opcode) {
                case 0: EX_stage.aluResult = registers[EX_stage.r2] + registers[EX_stage.r3]; break;
                case 1: EX_stage.aluResult = registers[EX_stage.r2] - registers[EX_stage.r3]; break;
                case 2: EX_stage.aluResult = registers[EX_stage.r2] * registers[EX_stage.r3]; break;
                case 3: EX_stage.aluResult = EX_stage.immediate; break;
                case 4: // Conditional branch
                    if (registers[EX_stage.r1] == registers[EX_stage.r2]) {
                        PC += 1 + EX_stage.immediate;
                        branch_taken = 1;
                    }
                    break;
                case 5: EX_stage.aluResult = registers[EX_stage.r2] & registers[EX_stage.r3]; break;
                case 6: EX_stage.aluResult = registers[EX_stage.r2] ^ EX_stage.immediate; break;
                case 7: // Unconditional jump
                    PC = (PC & 0xF0000000) | EX_stage.address;
                    branch_taken = 1;
                    break;
                case 8: EX_stage.aluResult = registers[EX_stage.r2] << EX_stage.r3; break;
                case 9: EX_stage.aluResult = registers[EX_stage.r2] >> EX_stage.r3; break;
                case 10:
                case 11:
                    EX_stage.aluResult = registers[EX_stage.r2] + EX_stage.immediate;
                    break;
            }
            if (branch_taken) {
                // Flush IF and ID, delay next fetch
                fetchNext = 0;
                fetchControl = 0;
                IF_stage.active = 0;
                ID_stage.active = 0;
                printf("EX: Branch taken, flushing IF and ID, new PC=%u\n", PC);
            }
            MEM_stage = EX_stage;
            MEM_stage.active = 1;
            EX_stage.active = 0;
        }

        // Decode Stage
        if (ID_stage.active) {
            uint32_t instr = ID_stage.instruction;
            ID_stage.opcode = (instr >> 28) & 0xF;
            if (ID_stage.opcode <= 2 || ID_stage.opcode == 5 ||
                ID_stage.opcode == 8 || ID_stage.opcode == 9) {
                ID_stage.r1 = (instr >> 23) & 0x1F;
                ID_stage.r2 = (instr >> 18) & 0x1F;
                ID_stage.r3 = (instr >> 13) & 0x1F;
            } else if (ID_stage.opcode == 7) {
                ID_stage.address = instr & 0x0FFFFFFF;
            } else {
                ID_stage.r1 = (instr >> 23) & 0x1F;
                ID_stage.r2 = (instr >> 18) & 0x1F;
                int imm = instr & 0x3FFFF;
                // Sign-extend 18-bit immediate
                if (imm & (1 << 17)) imm |= ~0x3FFFF;
                ID_stage.immediate = imm;
            }
            EX_stage = ID_stage;
            EX_stage.active = 1;
            ID_stage.active = 0;
        }

        // Fetch Stage
        if (fetchNext && PC < instruction_count) {
            IF_stage.instruction = memory[PC++];
            IF_stage.active = 1;
            fetchNext = 0;
        }
        if (IF_stage.active) {
            ID_stage = IF_stage;
            ID_stage.active = 1;
            IF_stage.active = 0;
        }

        // Advance cycle counters
        cycle++;
        fetchControl++;
        if (fetchControl == 2) {
            // Simulate 2-cycle execute delay
            fetchNext = 1;
            fetchControl = 0;
        }

        // Termination: no active stages & PC past last instruction
        if (!IF_stage.active && !ID_stage.active &&
            !EX_stage.active && !MEM_stage.active &&
            !WB_stage.active && PC >= instruction_count) {
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
