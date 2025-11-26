//===-- SodiumCallingConv.td - Calling Conventions Sodium --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the Sodium architecture.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Callee-saved register lists.
//===----------------------------------------------------------------------===//
// Default, $ra and $s0-$s11
def CSR_Default
    : CalleeSavedRegs<(add X2, X3, X8, X9, (sequence "X%u", 18, 27))>;

// Interrupt handler needs to save/restore all registers that are used,
// both Caller and Callee saved registers, except Stack Pointer.
def CSR_Interrupt
    : CalleeSavedRegs<(add X1, X2, X3, (sequence "X%u", 6, 31))>;

// Needed for implementation of SodiumRegisterInfo::getNoPreservedMask()
def CSR_NoReg
    : CalleeSavedRegs<(add)>;


//===----------------------------------------------------------------------===//
// Sodium Calling Convention
//===----------------------------------------------------------------------===//
def SodiumCC : CallingConv<[
  CCIfType<[i1, i8], CCPromoteToType<i16>>,
  // i16 arguments get passed in integer registers if there is space.
  CCIfType<[i16], CCAssignToReg<[X10, X11, X12, X13, X14, X15, X16, X17]>>, //a0-a7
  // As are v2i16 arguments (this would be the default behavior for
  // v2i16 if it wasn't allocated to the IntPair register-class)
  //CCIfType<[v2i16], CCCustom<"">>
  // Alternatively, they are assigned to the stack in 2-byte aligned units.
  CCAssignToStack<2, 2>
]>;
