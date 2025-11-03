add_llvm_component_group(Sodium)

set(LLVM_TARGET_DEFINITIONS Sodium.td)

tablegen(LLVM SodiumGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM SodiumGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM SodiumGenCallingConv.inc -gen-callingconv)
tablegen(LLVM SodiumGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM SodiumGenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM SodiumGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM SodiumGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM SodiumGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM SodiumGenMCPseudoLowering.inc -gen-pseudo-lowering)
tablegen(LLVM SodiumGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(SodiumCommonTableGen)

add_llvm_target(SodiumCodeGen
  SodiumTargetMachine.cpp
  SodiumTargetObjectFile.cpp
  SodiumISelDAGToDAG.cpp
  SodiumAsmPrinter.cpp
  SodiumSubtarget.cpp
  SodiumISelLowering.cpp
  SodiumCallLowering.cpp
  SodiumFrameLowering.cpp
  SodiumInstrInfo.cpp
  SodiumRegisterInfo.cpp
  SodiumExpandPseudoInsts.cpp
  SodiumMachineFunctionInfo.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  GlobalISel
  IPO
  MC
  SodiumDesc
  SodiumInfo
  SodiumDisassembler
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  Sodium
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
add_subdirectory(Disassembler)
