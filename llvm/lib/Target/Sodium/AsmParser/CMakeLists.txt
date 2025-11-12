add_llvm_component_library(LLVMSodiumAsmParser
  SodiumAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  SodiumDesc
  SodiumInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  Sodium
  )
