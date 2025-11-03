//===-- SodiumTargetInfo.h - Sodium Target Implementation -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SODIUM_SODIUMTARGETINFO_H
#define LLVM_SODIUM_SODIUMTARGETINFO_H

namespace llvm {

class Target;

Target &getTheSodium16Target();
Target &getTheSodium32Target();

} // namespace llvm

#endif // LLVM_LIB_TARGET_SODIUM_TARGETINFO_SODIUMTARGETINFO_H
