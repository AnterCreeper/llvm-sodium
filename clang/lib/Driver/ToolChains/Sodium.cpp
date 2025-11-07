//===--- Sodium.cpp - Sodium ToolChain Implementations ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Sodium.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Options.h"
#include "clang/Driver/Driver.h"
#include "llvm/Option/ArgList.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

SodiumToolChain::SodiumToolChain(const Driver &D, const llvm::Triple &Triple,
                                 const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
}

Tool *SodiumToolChain::buildLinker() const {
  return new tools::Sodium::Linker(getTriple(), *this);
}

void Sodium::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                  const InputInfo &Output,
                                  const InputInfoList &Inputs,
                                  const ArgList &Args,
                                  const char *LinkingOutput) const {
  std::string Linker = getToolChain().GetProgramPath(getShortName());

  ArgStringList CmdArgs;
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // Add library search paths before we specify libraries.
  Args.AddAllArgs(CmdArgs, options::OPT_L);
  getToolChain().AddFilePathLibArgs(Args, CmdArgs);
  AddLinkerInputs(getToolChain(), Inputs, Args, CmdArgs, JA);

  if (C.getDriver().isUsingLTO())
    addLTOOptions(getToolChain(), Args, CmdArgs, Output, Inputs[0],
                  C.getDriver().getLTOMode() == LTOK_Thin);

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  // Add user specified linker script.
  Args.AddAllArgs(CmdArgs, options::OPT_T);

  C.addCommand(std::make_unique<Command>(
      JA, *this, ResponseFileSupport::AtFileCurCP(), Args.MakeArgString(Linker),
      CmdArgs, Inputs, Output));
}
// Sodium tools end.
