add_llvm_component_library(LLVMSodiumInfo
  SodiumTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  Sodium
  )
