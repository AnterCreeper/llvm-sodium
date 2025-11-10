//===-- SodiumRegisterInfo.td - Sodium Register defs -------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the Sodium register files
//===----------------------------------------------------------------------===//
let Namespace = "Sodium" in {
class SodiumReg<bits<5> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{4-0} = Enc;
  let AltNames = alt;
}
def sub_even : SubRegIndex<16>;
def sub_odd  : SubRegIndex<16, 16>;
}

// Registers are identified with 5-bit ID numbers.
// 16-bit integer registers
class SodiumGPR<bits<5> Enc, string n, list<string> alt> : SodiumReg<Enc, n, alt>;

// Pairs of 16-bit integer registers
class SodiumGPRPair<bits<5> Enc, string n, list<string> alt, list<Register> subregs> : SodiumReg<Enc, n, alt> {
  let SubRegs = subregs;
  let SubRegIndices = [sub_even, sub_odd];
  let CoveredBySubRegs = 1;
}

def XLenVT : ValueTypeByHwMode<[Sodium16, Sodium32],
  [i16, v2i16]>;

// Integer registers
let FallbackRegAltNameIndex = NoRegAltName in
def ABIRegAltName : RegAltNameIndex;

let RegAltNameIndices = [ABIRegAltName] in {
  let isConstant = true in
  def X0  : SodiumGPR<0, "x0", ["zero"]>, DwarfRegNum<[0]>;
  def X1  : SodiumGPR<1, "x1", ["t5"]>,   DwarfRegNum<[1]>;
  def X2  : SodiumGPR<2, "x2", ["ra"]>,   DwarfRegNum<[2]>;
  def X3  : SodiumGPR<3, "x3", ["p0"]>,   DwarfRegNum<[3]>;
  def X4  : SodiumGPR<4, "x4", ["sp"]>,   DwarfRegNum<[4]>;
  def X5  : SodiumGPR<5, "x5", ["p1"]>,   DwarfRegNum<[5]>;
  def X6  : SodiumGPR<6, "x6", ["gp"]>,   DwarfRegNum<[6]>;
  def X7  : SodiumGPR<7, "x7", ["t4"]>,   DwarfRegNum<[7]>;
  def X8  : SodiumGPR<8, "x8", ["s0", "fp"]>, DwarfRegNum<[8]>;
  def X9  : SodiumGPR<9, "x9", ["s1"]>,   DwarfRegNum<[9]>;
  def X10 : SodiumGPR<10,"x10", ["a0"]>,  DwarfRegNum<[10]>;
  def X11 : SodiumGPR<11,"x11", ["a1"]>,  DwarfRegNum<[11]>;
  def X12 : SodiumGPR<12,"x12", ["a2"]>,  DwarfRegNum<[12]>;
  def X13 : SodiumGPR<13,"x13", ["a3"]>,  DwarfRegNum<[13]>;
  def X14 : SodiumGPR<14,"x14", ["a4"]>,  DwarfRegNum<[14]>;
  def X15 : SodiumGPR<15,"x15", ["a5"]>,  DwarfRegNum<[15]>;
  def X16 : SodiumGPR<16,"x16", ["a6"]>,  DwarfRegNum<[16]>;
  def X17 : SodiumGPR<17,"x17", ["a7"]>,  DwarfRegNum<[17]>;
  def X18 : SodiumGPR<18,"x18", ["s2"]>,  DwarfRegNum<[18]>;
  def X19 : SodiumGPR<19,"x19", ["s3"]>,  DwarfRegNum<[19]>;
  def X20 : SodiumGPR<20,"x20", ["s4"]>,  DwarfRegNum<[20]>;
  def X21 : SodiumGPR<21,"x21", ["s5"]>,  DwarfRegNum<[21]>;
  def X22 : SodiumGPR<22,"x22", ["s6"]>,  DwarfRegNum<[22]>;
  def X23 : SodiumGPR<23,"x23", ["s7"]>,  DwarfRegNum<[23]>;
  def X24 : SodiumGPR<24,"x24", ["s8"]>,  DwarfRegNum<[24]>;
  def X25 : SodiumGPR<25,"x25", ["s9"]>,  DwarfRegNum<[25]>;
  def X26 : SodiumGPR<26,"x26", ["s10"]>, DwarfRegNum<[26]>;
  def X27 : SodiumGPR<27,"x27", ["s11"]>, DwarfRegNum<[27]>;
  def X28 : SodiumGPR<28,"x28", ["t0"]>,  DwarfRegNum<[28]>;
  def X29 : SodiumGPR<29,"x29", ["t1"]>,  DwarfRegNum<[29]>;
  def X30 : SodiumGPR<30,"x30", ["t2"]>,  DwarfRegNum<[30]>;
  def X31 : SodiumGPR<31,"x31", ["t3"]>,  DwarfRegNum<[31]>;
}

// Pairs of Integer registers
def D0:   SodiumGPRPair<0,  "x0", ["zero32"], [X0, X1]>;
def D1:   SodiumGPRPair<2,  "x2", ["ra32"], [X2, X3]>;
def D2 :  SodiumGPRPair<4,  "x4", ["sp32"], [X4, X5]>;
def D3 :  SodiumGPRPair<6,  "x6", ["gp32"], [X6, X7]>;
def D4 :  SodiumGPRPair<8,  "x8", ["d4", "fp32"], [X8, X9]>;
def D5 :  SodiumGPRPair<10, "x10", ["d5"], [X10, X11]>;
def D6 :  SodiumGPRPair<12, "x12", ["d6"], [X12, X13]>;
def D7 :  SodiumGPRPair<14, "x14", ["d7"], [X14, X15]>;
def D8 :  SodiumGPRPair<16, "x16", ["d8"], [X16, X17]>;
def D9 :  SodiumGPRPair<18, "x18", ["d9"], [X18, X19]>;
def D10 : SodiumGPRPair<20, "x20", ["d10"], [X20, X21]>;
def D11 : SodiumGPRPair<22, "x22", ["d11"], [X22, X23]>;
def D12 : SodiumGPRPair<24, "x24", ["d12"], [X24, X25]>;
def D13 : SodiumGPRPair<26, "x26", ["d13"], [X26, X27]>;
def D14 : SodiumGPRPair<28, "x28", ["d14"], [X28, X29]>;
def D15 : SodiumGPRPair<30, "x30", ["d15"], [X30, X31]>;

// The order of registers represents the preferred allocation sequence.
// Registers are listed in the order caller-save, callee-save, specials.
def IntRegs : RegisterClass<"Sodium", [i16], 16,
                            (add (sequence "X%u", 10, 17),  //a0-a7
                                 (sequence "X%u", 28, 31),  //t0-t3
                                 (sequence "X%u", 8, 9),    //s0-s1
                                 (sequence "X%u", 18, 27),  //s2-s11
                                 (sequence "X%u", 0, 7))>;  //zero, ra, sp, tp

def IntPair : RegisterClass<"Sodium", [v2i16], 32,
                            (add (sequence "D%u", 5, 8),    //a0-a7
                                  D14, D15,                 //t0-t3
                                  D4,                       //s0-s1
                                 (sequence "D%u", 9, 13),   //s2-s11
                                 (sequence "D%u", 0, 3))>;  //zero, ra, sp, tp

def RegsNoX0 : RegisterClass<"Sodium", [i16], 16, (sub IntRegs, X0)>;
def PairNoX0 : RegisterClass<"Sodium", [v2i16], 32, (sub IntPair, D0)>;
