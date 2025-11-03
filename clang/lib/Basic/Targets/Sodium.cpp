//===--- Sodium.cpp - Implement Sodium target feature support -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Sodium TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "Sodium.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;
using namespace clang::targets;

/*
const Builtin::Info SodiumTargetInfo::BuiltinInfo[] = {
#define BUILTIN(ID, TYPE, ATTRS) \
  {#ID, TYPE, ATTRS, nullptr, ALL_LANGUAGES, nullptr},
#define LIBBUILTIN(ID, TYPE, ATTRS, HEADER) \
  {#ID, TYPE, ATTRS, HEADER, ALL_LANGUAGES, nullptr},
#include "clang/Basic/BuiltinsSodium.def"
};
*/

ArrayRef<Builtin::Info> SodiumTargetInfo::getTargetBuiltins() const {
//  return llvm::makeArrayRef(BuiltinInfo, clang::Sodium::LastTSBuiltin -
//    Builtin::FirstTSBuiltin);
  return std::nullopt;
}

ArrayRef<const char *> SodiumTargetInfo::getGCCRegNames() const {
  static const char *const GCCRegNames[] = {
      // Integer registers
      "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
      "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
      "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
      "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31"};
  return llvm::ArrayRef(GCCRegNames);
}

ArrayRef<TargetInfo::GCCRegAlias> SodiumTargetInfo::getGCCRegAliases() const {
  static const TargetInfo::GCCRegAlias GCCRegAliases[] = {
      {{"zero"}, "x0"}, {{"r0"}, "x1"},   {{"ra"}, "x2"},    {{"r1"}, "x3"},
      {{"sp"}, "x4"},   {{"r2"}, "x5"},   {{"tp"}, "x6"},    {{"r3"}, "x7"},
      {{"s0"}, "x8"},   {{"s1"}, "x9"},   {{"a0"}, "x10"},   {{"a1"}, "x11"},
      {{"a2"}, "x12"},  {{"a3"}, "x13"},  {{"a4"}, "x14"},   {{"a5"}, "x15"},
      {{"a6"}, "x16"},  {{"a7"}, "x17"},  {{"s2"}, "x18"},   {{"s3"}, "x19"},
      {{"s4"}, "x20"},  {{"s5"}, "x21"},  {{"s6"}, "x22"},   {{"s7"}, "x23"},
      {{"s8"}, "x24"},  {{"s9"}, "x25"},  {{"s10"}, "x26"},  {{"s11"}, "x27"},
      {{"t0"}, "x28"},  {{"t1"}, "x29"},  {{"t2"}, "x30"},   {{"t3"}, "x31"}};
  return llvm::ArrayRef(GCCRegAliases);
}

void SodiumTargetInfo::getTargetDefines(const LangOptions &Opts,
                                       MacroBuilder &Builder) const {
  Builder.defineMacro("__sodium");
  bool Is32Bit = getTriple().getArch() == llvm::Triple::sodium32;
  Builder.defineMacro("__sodium_xlen", Is32Bit ? "32" : "16");
}
