add_llvm_component_library(LLVMSodiumDisassembler
  SodiumDisassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  SodiumDesc
  SodiumInfo
  Support

  ADD_TO_COMPONENT
  Sodium
  )
