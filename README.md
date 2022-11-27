# PC-RT-Emulator

The goal of this project is to emulate the IBM PC RT architecture and I/O.
This project is written in C and uses SDL2 for rendering a GUI.

### TODOs:
- DONE ~~Implement barebones arch, mmu, memory, rom with a robust logging system. This allows first execution of ROM code and disassembling this code on the fly in a log file.~~
- DONE ~~Implement GUI to assist in debugging and get bare bones of GUI code for MDA rendering implemented.~~
- DONE ~~Implement basic IO decoding and memory to pass some early tests and init of MDA (MDA isn't fully implemented, just regs and memory).~~
- DONE ~~Implement address translation and memory protection.~~
- DONE ~~Add ECC generation and checking.~~
- DONE ~~Add Interrupt support, Program Check support, and Machine Check support.~~
- Current Task: *Stumble through processor init rom tests implementing features and fixing instruction bugs as tests fail. (this will only be marked DONE when all tests pass)*
- Implement I/O: FDC, MDA, KB being the first few along with any required system board I/O to get first signs of system interaction. (Boot Diag disk?)
- Add either vscode config files to allow simple building on other machines that way or some automated build system in actions to build project for multiple platforms. (Currently built on an M1 Macbook Air but code should be portable.)
