# Project : ST4Ever : The Revival Engine for the Timeless ATARI ST

*(Current project state: UC1 validated - see CLAUDE.md for more information)*

This project is a cross-platform interactive console application developed in pure C for educational purposes allowing to:

- read/write ATARI ST files (text or binary .PRG, .TTP, .TOS)
- create/read ATARI ST disk images of types .st, .msa, .stx from/to PC directories
- modify, compile, assemble, disassemble, decompile ATARI ST programs
- emulate ATARI ST binaries through various views (CPU, Memory, Screen) in step-by-step mode, slowed time, or real-time 

The project views, launched by the interactive console commands, include:

- a text file and/or source file editor for a binary (e.g. assembler source of a .PRG)
- a hexadecimal editor for editing binaries (e.g. a .PRG, .TTP)
- a 68000 assembler view in DevPac3 ATARI ST format for the relevant binary sections of the hexadecimal view (e.g. .text or any address range provided by the user)
- an execution monitor for ATARI ST binaries allowing step-by-step selection, stop, go, execution time per instruction
- a 68000 CPU viewer with register states for binary execution
- an ATARI ST memory viewer for binary execution
- a simple ATARI ST screen/inputs/outputs emulator for binary execution
- a file/directory tree viewer for file and disk image management and selection (.st, .msa, .stx)
- a trace/logs/errors console for the application and for ST4Ever developer debugging

Note that the ST4Ever ATARI ST emulation is completely developed from scratch for educational purposes: Hatari and STeem are very good complete and efficient emulators. The ST4Ever ATARI ST emulation is simplified to meet the needs of step-by-step execution of the functions mentioned above.

Future evolutions of the project will include ancillary utilities such as a GEN.TTP version of Devpac3 ported to PC for generating binaries on PC without using an emulator like Hatari or STeem (Vincent Rivière's m68k-atari-mint cross-tools also exist and is excellent, but once again, educational purpose is the goal of this project, not the use of existing programs). Also, the development of a 68000 assembler to pure C decompiler to recompile the program under msys2 on PC.

The ultimate goal is to produce pure C source code compiled under Msys2 from ATARI ST demo disk images, to compile them in PC format and run them under Msys2 in a graphical Windows or Linux window without ATARI ST emulation (hence the "revival"...).

In terms of code production and project follow-up, this repository is assisted by Claude AI for educational purposes and my own understanding of the use of an AI assistant, ATARI ST emulation and demos development from 1990's.

The main project file shared with Claude AI is SPEC-fr.md (in french) - but any AI assistant will translate it for you. Ssuccessive versions of the project file are created from my inputs and Claude's inputs; the change history will indicate which version is last updated by myself or Claude.

Note that this file is fully written manually - at least in its first issue ;)
