//===-- SodiumBaseInfo.cpp - Top level definitions for Sodium MC ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone enum definitions for the Sodium target
// useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//

#include "SodiumBaseInfo.h"

namespace llvm {

namespace SodiumSysReg {

#define GET_SysRegsList_IMPL
#include "SodiumGenSearchableTables.inc"

} // namespace SodiumSysReg

} //end namespace llvm
