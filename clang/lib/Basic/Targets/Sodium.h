//===--- Sodium.h - Declare Sodium target feature support -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares Sodium TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_SODIUM_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_SODIUM_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

// Sodium Target
class SodiumTargetInfo : public TargetInfo {
  static const Builtin::Info BuiltinInfo[];

public:
  SodiumTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    SuitableAlign = 128;
    IntWidth = IntAlign = 16;     //int:      16b
    LongWidth = LongAlign = 32;   //long:     32b
    LongLongWidth = LongLongAlign = 64;
				  //longlong: 64b
    WIntType = UnsignedLong;      //wint_t:   long, 32b
    WCharType = SignedLong;       //wchat_t:  long, 32b
    IntMaxType = SignedLongLong;  //intmax:   longlong, 64b
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
  ArrayRef<Builtin::Info> getTargetBuiltins() const override;

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  std::string_view getClobbers() const override { return ""; }

  ArrayRef<const char *> getGCCRegNames() const override;
  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override;

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    return false;
  }
};
class LLVM_LIBRARY_VISIBILITY Sodium16TargetInfo : public SodiumTargetInfo {
public:
  Sodium16TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : SodiumTargetInfo(Triple, Opts) {
    SizeType = UnsignedInt;
    IntPtrType = SignedInt;
    PtrDiffType = SignedInt;
    PointerWidth = PointerAlign = 16;
    // Description string has to be kept in sync with backend string at
    // llvm/lib/Target/<Arch>/<Arch>TargetMachine.cpp
    resetDataLayout("e"
    // ELF name mangling
    "-m:e"
    // 16-bit pointers, 16-bit aligned
    "-p:16:16"
    // 32-bit integers, 32-bit aligned
    "-i32:32"
    // 16-bit and 32-bit native integer width
    "-n16:32"
    // 128-bit natural stack alignment, in 16 Bytes
    "-S128"
    );
  }
};
class LLVM_LIBRARY_VISIBILITY Sodium32TargetInfo : public SodiumTargetInfo {
public:
  Sodium32TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : SodiumTargetInfo(Triple, Opts) {
    SizeType = UnsignedLong;
    IntPtrType = SignedLong;
    PtrDiffType = SignedLong;
    PointerWidth = PointerAlign = 32;
    // Description string has to be kept in sync with backend string at
    // llvm/lib/Target/<Arch>/<Arch>TargetMachine.cpp
    resetDataLayout("e"
    // ELF name mangling
    "-m:e"
    // 32-bit pointers, 32-bit aligned
    "-p:32:32"
    // 32-bit integers, 32-bit aligned
    "-i32:32"
    // 16-bit and 32-bit native integer width
    "-n16:32"
    // 128-bit natural stack alignment, in 16 Bytes
    "-S128"
    );
  }
};
} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_SODIUM_H
