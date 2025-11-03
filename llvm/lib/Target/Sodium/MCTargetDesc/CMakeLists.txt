add_llvm_component_library(LLVMSodiumDesc
  SodiumMCTargetDesc.cpp
  SodiumInstPrinter.cpp
  SodiumTargetStreamer.cpp
  SodiumMCAsmInfo.cpp
  SodiumMCCodeEmitter.cpp
  SodiumELFObjectWriter.cpp
  SodiumAsmBackend.cpp
  SodiumMCExpr.cpp

  LINK_COMPONENTS
  MC
  SodiumInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  Sodium
  )
