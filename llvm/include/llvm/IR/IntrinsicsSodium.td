//===- IntrinsicsSodium.td - Defines Sodium intrinsics -----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Sodium-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "sodium" in {
  // T @llvm.<name>.i16.<p>(i16, i16);
  def int_sodium_tbe  : ClangBuiltin<"__builtin_sodium_tbe">,
    DefaultAttrsIntrinsic<[llvm_i16_ty], [llvm_i16_ty, llvm_i16_ty], [IntrNoMem]>;
  // T @llvm.<name>.i16.<p>(i16, i16, imm);
  def int_sodium_pack : ClangBuiltin<"__builtin_sodium_pack">,
    DefaultAttrsIntrinsic<[llvm_i16_ty], [llvm_i16_ty, llvm_i16_ty, llvm_i16_ty], [IntrNoMem, ImmArg<ArgIndex<2>>]>;
}

//internal intrinsics
let TargetPrefix = "sodium" in {
  def int_sodium_mul32  : Intrinsic<[llvm_v2i16_ty], [llvm_i16_ty, llvm_i16_ty], [IntrNoMem]>;
  def int_sodium_mulu32 : Intrinsic<[llvm_v2i16_ty], [llvm_i16_ty, llvm_i16_ty], [IntrNoMem]>;
}
