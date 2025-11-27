//=- SodiumMachineFunctionInfo.h - Sodium machine function info ---*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Sodium-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMMACHINEFUNCTIONINFO_H
#define LLVM_SODIUM_SODIUMMACHINEFUNCTIONINFO_H

#include "SodiumSubtarget.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class SodiumMachineFunctionInfo;

namespace yaml {
struct SodiumMachineFunctionInfo final : public yaml::MachineFunctionInfo {
  int VarArgsFrameIndex;
  int VarArgsSaveSize;

  SodiumMachineFunctionInfo() = default;
  SodiumMachineFunctionInfo(const llvm::SodiumMachineFunctionInfo &MFI);

  void mappingImpl(yaml::IO &YamlIO) override;
  ~SodiumMachineFunctionInfo() = default;
};

template <> struct MappingTraits<SodiumMachineFunctionInfo> {
  static void mapping(IO &YamlIO, SodiumMachineFunctionInfo &MFI) {
    YamlIO.mapOptional("varArgsFrameIndex", MFI.VarArgsFrameIndex);
    YamlIO.mapOptional("varArgsSaveSize", MFI.VarArgsSaveSize);
  }
};
} // end namespace yaml

/// SodiumMachineFunctionInfo - This class is derived from MachineFunctionInfo
/// and contains private Sodium-specific information for each MachineFunction.
class SodiumMachineFunctionInfo : public MachineFunctionInfo {
private:
  /// FrameIndex for start of varargs area
  int VarArgsFrameIndex = 0;
  /// Size of the save area used for varargs
  int VarArgsSaveSize = 0;
  /// Size of any opaque stack adjustment due to save/restore libcalls.
  unsigned LibCallStackSize = 0;
  /// Size of stack frame to save callee saved registers
  unsigned CalleeSavedStackSize = 0;

public:
  SodiumMachineFunctionInfo(const Function &F, const TargetSubtargetInfo *STI) {}

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  unsigned getVarArgsSaveSize() const { return VarArgsSaveSize; }
  void setVarArgsSaveSize(int Size) { VarArgsSaveSize = Size; }

  unsigned getLibCallStackSize() const { return LibCallStackSize; }
  void setLibCallStackSize(unsigned Size) { LibCallStackSize = Size; }

  unsigned getCalleeSavedStackSize() const { return CalleeSavedStackSize; }
  void setCalleeSavedStackSize(unsigned Size) { CalleeSavedStackSize = Size; }

  void initializeBaseYamlFields(const yaml::SodiumMachineFunctionInfo &YamlMFI);

};

} // end namespace llvm

#endif // LLVM_SODIUM_SODIUMMACHINEFUNCTIONINFO_H
