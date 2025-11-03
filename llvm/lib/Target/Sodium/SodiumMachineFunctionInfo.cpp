//=- SodiumMachineFunctionInfo.cpp - Sodium machine function info -*- C++ -*-=//
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

#include "SodiumMachineFunctionInfo.h"

using namespace llvm;

yaml::SodiumMachineFunctionInfo::SodiumMachineFunctionInfo(
    const llvm::SodiumMachineFunctionInfo &MFI)
    : VarArgsFrameIndex(MFI.getVarArgsFrameIndex()),
      VarArgsSaveSize(MFI.getVarArgsSaveSize()) {}

MachineFunctionInfo *SodiumMachineFunctionInfo::clone(
    BumpPtrAllocator &Allocator, MachineFunction &DestMF,
    const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
    const {
  return DestMF.cloneInfo<SodiumMachineFunctionInfo>(*this);
}

void yaml::SodiumMachineFunctionInfo::mappingImpl(yaml::IO &YamlIO) {
  MappingTraits<SodiumMachineFunctionInfo>::mapping(YamlIO, *this);
}

void SodiumMachineFunctionInfo::initializeBaseYamlFields(
    const yaml::SodiumMachineFunctionInfo &YamlMFI) {
  VarArgsFrameIndex = YamlMFI.VarArgsFrameIndex;
  VarArgsSaveSize = YamlMFI.VarArgsSaveSize;
}
