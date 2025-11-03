//===--- Sodium.h - Sodium ToolChain Implementations ------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_SODIUM_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_SODIUM_H

#include "Gnu.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Tool.h"
#include "clang/Driver/ToolChain.h"

namespace clang {
namespace driver {
namespace toolchains {
class LLVM_LIBRARY_VISIBILITY SodiumToolChain : public Generic_ELF {
public:
  SodiumToolChain(const Driver &D, const llvm::Triple &Triple,
                  const llvm::opt::ArgList &Args);

  bool HasNativeLLVMSupport() const override { return true; }
protected:
//  Tool *buildLinker() const override;

private:

};

} // end namespace toolchains

/*
namespace tools {
namespace Sodium {
class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  Linker(const llvm::Triple &Triple, const ToolChain &TC)
      : Tool("Sodium::Linker", "sodium-ld", TC), Triple(Triple) {}

  bool isLinkJob() const override { return true; }
  bool hasIntegratedCPP() const override { return false; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;

protected:
  const llvm::Triple &Triple;

};

} // end namespace Sodium
} // end namespace tools
*/
} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_SODIUM_H
