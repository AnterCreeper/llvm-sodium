//===-- SodiumTargetMachine.h - Define TargetMachine for Sodium -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the Sodium specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_TARGETMACHINE_H
#define LLVM_SODIUM_TARGETMACHINE_H

#include "SodiumSubtarget.h"
#include "llvm/Target/TargetMachine.h"
#include <optional>

namespace llvm {
class SodiumTargetMachine : public LLVMTargetMachine {
  bool is32Bit;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  SodiumSubtarget Subtarget;

public:
  SodiumTargetMachine(const Target &T, const Triple &TT,
                      StringRef CPU, StringRef FS,
                      const TargetOptions &Options,
                      std::optional<Reloc::Model> RM,
                      std::optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT,
                      bool is32Bit);
  ~SodiumTargetMachine() override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
  bool is32Mode() const { return is32Bit; }
  const SodiumSubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }
  // DO NOT IMPLEMENT: There is no such thing as a valid default subtarget,
  // subtargets are per-function entities based on the target-specific
  // attributes of each function.
  const SodiumSubtarget *getSubtargetImpl() const = delete;
};

// Sodium 16-bit target machine
class Sodium16TargetMachine : public SodiumTargetMachine {
  virtual void anchor();
public:
  Sodium16TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      std::optional<Reloc::Model> RM,
                      std::optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT);
};
// Sodium 32-bit target machine
class Sodium32TargetMachine : public SodiumTargetMachine {
  virtual void anchor();
public:
  Sodium32TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                        StringRef FS, const TargetOptions &Options,
                        std::optional<Reloc::Model> RM,
                        std::optional<CodeModel::Model> CM,
                        CodeGenOpt::Level OL, bool JIT);
};
} // namespace llvm
#endif
