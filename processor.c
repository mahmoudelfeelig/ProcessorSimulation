#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_SIZE     2048
#define REGISTER_COUNT  32

/* ─── Global state ─────────────────── */
uint32_t memory[MEMORY_SIZE];
uint32_t registers[REGISTER_COUNT];
uint32_t PC              = 0;
int      instruction_cnt = 0;
int      globalCycle     = 0;

/* ─── Pipeline stage structure ─────── */
typedef struct {
    /* raw instruction + timing */
    uint32_t raw;
    int      valid;          /* 1 = stage holds an instruction                */
    int      stageCycle;     /* counts DOWN – remaining cycles *inside* stage */
    /* decoded fields (only meaningful after decode) */
    int opcode;
    int r1, r2, r3;
    int imm;
    int addr;
    uint32_t aluResult;
    uint32_t memResult;
} Stage;

/* ─── Five stages ───────────────────── */
Stage IF, ID, EX, MEM, WB;

/* ─── Helpers ───────────────────────── */
int getOpcode(const char *m) {
    if (!strcmp(m, "ADD"))  return 0;
    if (!strcmp(m, "SUB"))  return 1;
    if (!strcmp(m, "MUL"))  return 2;
    if (!strcmp(m, "MOVI")) return 3;
    if (!strcmp(m, "JEQ"))  return 4;
    if (!strcmp(m, "AND"))  return 5;
    if (!strcmp(m, "XORI")) return 6;
    if (!strcmp(m, "JMP"))  return 7;
    if (!strcmp(m, "LSL"))  return 8;
    if (!strcmp(m, "LSR"))  return 9;
    if (!strcmp(m, "MOVR")) return 10;
    if (!strcmp(m, "MOVM")) return 11;
    return -1;
}

void initialize(void) {
    memset(memory,    0, sizeof(memory));
    memset(registers, 0, sizeof(registers));
    registers[0] = 0;      /* R0 hard-wired to 0 */
    PC = 0;
}

/* ─── Assembly loader (plain text) ─── */
void loadInstructions(const char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp) { perror("open"); exit(1); }

    char line[128], mnem[8];
    int  addr = 0;
    while (fgets(line, sizeof line, fp)) {

        // comments removal logic
        if (line[0] == ';' || line[0] == '\n' || line[0] == '\0') continue; // to skip comments in program.txt
        char *comment = strchr(line, ';');
        if (comment) *comment = '\0';

        int op, r1, r2, r3, imm, j;
        uint32_t ins = 0;
        if (sscanf(line, "%s", mnem) != 1) continue;
        op  = getOpcode(mnem);
        ins |= (op & 0xF) << 28;

        if (op <= 2 || op == 5 || op == 8 || op == 9) {          /* R-type / shift */
            sscanf(line, "%*s R%d R%d R%d", &r1, &r2, &r3);
            ins |= (r1 & 0x1F) << 23;
            ins |= (r2 & 0x1F) << 18;
            if (op == 8 || op == 9)
                ins |= (r3 & 0x1FFF);
            else
                ins |= (r3 & 0x1F) << 13;
        } else if (op == 7) {                                    /* JMP */
            sscanf(line, "%*s %d", &j);
            ins |= (j & 0x0FFFFFFF);
        } else {                                                 /* I-type */
            sscanf(line, "%*s R%d R%d %d", &r1, &r2, &imm);
            ins |= (r1 & 0x1F) << 23;
            ins |= (r2 & 0x1F) << 18;
            ins |= (imm & 0x3FFFF);
        }
        memory[addr++] = ins;
    }
    fclose(fp);
    instruction_cnt = addr;
}

/* ─── Decode helper ─────────────────── */
void decode(Stage *s) {
    uint32_t ins = s->raw;
    s->opcode = (ins >> 28) & 0xF;

    if (s->opcode <= 2 || s->opcode == 5 || s->opcode == 8 || s->opcode == 9) {
        s->r1 = (ins >> 23) & 0x1F;
        s->r2 = (ins >> 18) & 0x1F;
        s->r3 = (ins >> 13) & 0x1F;
    } else if (s->opcode == 7) {    /* JMP */
        s->addr = ins & 0x0FFFFFFF;
    } else {
        s->r1  = (ins >> 23) & 0x1F;
        s->r2  = (ins >> 18) & 0x1F;
        s->imm = ins & 0x3FFFF;
        if (s->imm & (1 << 17)) s->imm |= ~0x3FFFF;   /* sign-extend */
    }
}

/* ─── Pretty printing ───────────────── */
void printPipeline(void) {
    printf("\n+-------+---------------------------+\n");
    printf("| Stage | Instruction              |\n");
    printf("+-------+---------------------------+\n");
    const char *lab[5] = {"IF","ID","EX","MEM","WB"};
    Stage *st[5] = {&IF,&ID,&EX,&MEM,&WB};
    for (int i=0;i<5;i++) {
        printf("| %-5s | ", lab[i]);
        if (st[i]->valid) {
            uint32_t raw = st[i]->raw;
            int op  = (raw >> 28) & 0xF;
            int r1  = (raw >> 23) & 0x1F;
            int r2  = (raw >> 18) & 0x1F;
            int r3  = (raw >> 13) & 0x1F;
            int imm = raw & 0x3FFFF;
            int adr = raw & 0x0FFFFFFF;
            switch(op){
                case 0:  printf("ADD  R%d R%d R%d",   r1,r2,r3); break;
                case 1:  printf("SUB  R%d R%d R%d",   r1,r2,r3); break;
                case 2:  printf("MUL  R%d R%d R%d",   r1,r2,r3); break;
                case 3:  printf("MOVI R%d R%d %d",    r1,r2,(imm&(1<<17))?(imm|~0x3FFFF):imm); break;
                case 4:  printf("JEQ  R%d R%d %d",    r1,r2,(imm&(1<<17))?(imm|~0x3FFFF):imm); break;
                case 5:  printf("AND  R%d R%d R%d",   r1,r2,r3); break;
                case 6:  printf("XORI R%d R%d %d",    r1,r2,(imm&(1<<17))?(imm|~0x3FFFF):imm); break;
                case 7:  printf("JMP  %d",            adr); break;
                case 8:  printf("LSL  R%d R%d %d",    r1,r2,r3); break;
                case 9:  printf("LSR  R%d R%d %d",    r1,r2,r3); break;
                case 10: printf("MOVR R%d R%d %d",    r1,r2,(imm&(1<<17))?(imm|~0x3FFFF):imm); break;
                case 11: printf("MOVM R%d R%d %d",    r1,r2,(imm&(1<<17))?(imm|~0x3FFFF):imm); break;
                default: printf("???"); break;
            }
        } else {
            printf("---");
        }
        printf("\n");
    }
    printf("+-------+---------------------------+\n");
}

void printRegisters(void) {
    printf("\n=== Register Dump ===\n");
    for (int i=0;i<REGISTER_COUNT;i++)
        printf("R%-2d = %d\n", i, registers[i]);
    printf("PC  = %d\n", PC);
}

void printDataMem(void) {
    printf("\n=== Data Memory Dump (non-zero entries) ===\n");
    int nz = 0;
    for (int i=instruction_cnt;i<MEMORY_SIZE;i++)
        if (memory[i]) { printf("M[%d] = %d\n", i, memory[i]); nz=1; }
    if (!nz) puts("(all data memory is 0)");
}

/* ─── Main simulation loop ─────────── */
void simulate(void) {
    int fetchStall = 0;          /* 2-cycle stall after branch */
    int active     = 1;

    while (active) {
        printf("\nClock Cycle %d\n", ++globalCycle);

        /* ===== 1. WRITE-BACK ===== */
        if (WB.valid) {
            if (WB.opcode<=2 || WB.opcode==3 || WB.opcode==5 || WB.opcode==6 ||
                WB.opcode==8 || WB.opcode==9) {
                if (WB.r1) { registers[WB.r1] = WB.aluResult;
                             printf("WB: R%d = %d\n", WB.r1, WB.aluResult);}
            } else if (WB.opcode==10) {          /* load */
                if (WB.r1) { registers[WB.r1] = WB.memResult;
                             printf("WB: R%d loaded %d\n",WB.r1,WB.memResult);}
            }
            WB.valid = 0;
        }

        /* ===== 2. MEMORY ===== */
        if (MEM.valid) {
            if (MEM.opcode==10) {                         /* MOVR load */
                MEM.memResult = memory[MEM.aluResult];
            } else if (MEM.opcode==11) {                  /* MOVM store */
                memory[MEM.aluResult] = registers[MEM.r1];
                printf("MEM: M[%d] = %d\n", MEM.aluResult, registers[MEM.r1]);
            }
            WB = MEM; WB.valid = 1;
            MEM.valid = 0;
        }

        /* ===== 3. EXECUTE ===== */
        if (EX.valid) {
            if (--EX.stageCycle == 0) {   /* finished 2-cycle latency */
                int taken = 0;            /* branch flag            */
                switch(EX.opcode){
                    case 0:  EX.aluResult = registers[EX.r2] + registers[EX.r3]; break;
                    case 1:  EX.aluResult = registers[EX.r2] - registers[EX.r3]; break;
                    case 2:  EX.aluResult = registers[EX.r2] * registers[EX.r3]; break;
                    case 3:  EX.aluResult = EX.imm;                                break;
                    case 4:  if (registers[EX.r1]==registers[EX.r2]) {            /* JEQ */
                                PC += 1 + EX.imm; taken=1; }
                              break;
                    case 5:  EX.aluResult = registers[EX.r2] & registers[EX.r3]; break;
                    case 6:  EX.aluResult = registers[EX.r2] ^ EX.imm;            break;
                    case 7:  PC = (PC & 0xF0000000) | EX.addr; taken=1;            break;
                    case 8:  EX.aluResult = registers[EX.r2] << EX.r3;            break;
                    case 9:  EX.aluResult = registers[EX.r2] >> EX.r3;            break;
                    case 10:
                    case 11: EX.aluResult = registers[EX.r2] + EX.imm;            break;
                }
                if (taken) {                     /* flush IF & ID, stall fetch 2 cycles */
                    IF.valid = ID.valid = 0;
                    fetchStall = 2;
                    printf("Branch taken → flush IF/ID, PC=%d\n", PC);
                }
                MEM = EX; MEM.valid = 1;         /* send to MEM */
                EX.valid = 0;
            }
        }

        /* ===== 4. DECODE ===== */
        if (ID.valid) {
            if (--ID.stageCycle == 0 && !EX.valid) {      /* ready & EX free */
                Stage tmp = ID;                           /* copy to tmp to decode */
                decode(&tmp);
                tmp.stageCycle = 2;
                EX = tmp; EX.valid = 1;
                ID.valid = 0;
            }
        }

        /* ===== 5. FETCH ===== */
        if (fetchStall > 0) fetchStall--;
        else if (!IF.valid && PC < instruction_cnt) {     /* fetch if slot free */
            IF.raw   = memory[PC++];
            IF.valid = 1;
        }

        /* ===== 6. IF → ID transfer (next cycle only) === */
        static int firstCycleComplete = 0;
        if (firstCycleComplete && IF.valid && !ID.valid) {
            ID = IF;
            ID.valid = 1;
            ID.stageCycle = 2;
            IF.valid = 0;
        }
        firstCycleComplete = 1;


        /* ===== Print pipeline & registers ============= */
        printPipeline();
        printRegisters();

        /* ===== Termination check ====================== */
        if (!IF.valid && !ID.valid && !EX.valid &&
            !MEM.valid && !WB.valid &&
            PC >= instruction_cnt && fetchStall==0)
            active = 0;
    }
}

/* ─── main ─────────────────────────── */
int main(void) {
    initialize();
    loadInstructions("program.txt");
    puts("\n=== Starting Pipeline Simulation ===");
    simulate();
    puts("\n=== Simulation Complete ===");
    printRegisters();
    printDataMem();
    return 0;
}
