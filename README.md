# ProcessorSimulation

A complete simulation of a custom-designed processor, built from scratch to emulate instruction execution, data flow, memory handling, and control logic. This project showcases a full educational pipeline, including an assembler, datapath simulation, and test programs, ideal for learning computer architecture fundamentals.

## Features

- Custom instruction set architecture (ISA)
- Simulated memory and register file
- ALU and control unit logic
- Support for various instruction types: arithmetic, memory, control
- Test programs to validate execution
- Modular and readable C++ code

## Architecture Overview

This processor follows a typical 5-stage pipeline architecture:

1. Fetch: retrieve instructions from memory  
2. Decode: interpret opcode, resolve operands  
3. Execute: perform ALU operations  
4. Memory: read/write from simulated memory  
5. Writeback: store results in registers  

Registers, memory, and control logic are all modeled in code, making it easy to extend or tweak.

## Requirements

- C++11 or higher
- A C++ compiler (e.g. `g++` or `clang++`)
- Make (optional, for build automation)

## How to Run

1. **Clone the repo:**
   ```bash
   git clone https://github.com/mahmoudelfeelig/ProcessorSimulation.git
   cd ProcessorSimulation
   ```

2. **Build the project:**
   ```bash
   g++ -std=c++11 -o processor main.cpp
   ```

   Or use the provided `Makefile` if available:
   ```bash
   make
   ```

3. **Run the simulation:**
   ```bash
   ./processor
   ```

You can edit or replace the instruction input in the source to test different scenarios.

## Educational Goals

This project is intended for:

- Learning and teaching processor architecture
- Experimenting with custom ISAs
- Simulating basic computer components in software
- Gaining hands-on understanding of how hardware works

## License

This project is open-source under the [MIT License](LICENSE).
