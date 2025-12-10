//===-- SodiumInstrInfo.td - Target Description for Sodium Target ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the Sodium instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction format superclass
//===----------------------------------------------------------------------===//

include "SodiumInstrFormats.td"

//===----------------------------------------------------------------------===//
// Custom ISD SDNode Definition, at SodiumISelLowering.h
//===----------------------------------------------------------------------===//
// SDTypeProfile<numresults, numoperands, constraints>
def SDTCall : SDTypeProfile<0, -1, [SDTCisVT<0, XLenVT>]>;
def SDTCallSeq_End : SDCallSeqEnd<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;
def SDTCallSeq_Start : SDCallSeqStart<[SDTCisVT<0, i16>, SDTCisVT<1, i16>]>;

// Custom nodes
def sodium_li : SDNode<"SodiumISD::LI", SDTIntUnaryOp>;
def sodium_la : SDNode<"SodiumISD::LA", SDTIntUnaryOp>;
def sodium_ret :  SDNode<"SodiumISD::Ret",  SDTNone,
  [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def sodium_eret : SDNode<"SodiumISD::ERet", SDTNone,
  [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def sodium_call : SDNode<"SodiumISD::Call", SDTCall,
  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def sodium_tail : SDNode<"SodiumISD::Tail", SDTCall,
  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;

// Intrinsics
def sodium_tbe : SDNode<"SodiumISD::TBE", SDTIntBinOp>;

// Target-independent nodes, but with target-specific formats.
def callseq_end : SDNode<"ISD::CALLSEQ_END", SDTCallSeq_End,
  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDTCallSeq_Start,
  [SDNPHasChain, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// Register Operand Definition
//===----------------------------------------------------------------------===//
def MemRegs : RegisterOperand<IntRegs>{
  let OperandType = "OPERAND_MEMORY";
}

//===----------------------------------------------------------------------===//
// Immediate Operand Class Templates
//===----------------------------------------------------------------------===//
class ImmAsmOperand<string prefix, int width, string suffix> : AsmOperandClass {
  let Name = prefix # "Imm" # width # suffix;
  let RenderMethod = "addImmOperands";
}

class SImmAsmOperand<int width, string suffix = "">
  : ImmAsmOperand<"S", width, suffix> {
}

class UImmAsmOperand<int width, string suffix = "">
  : ImmAsmOperand<"U", width, suffix> {
}

def CSRSystemRegister : AsmOperandClass {
  let Name = "CSRSystemRegister";
  let ParserMethod = "parseCSRSystemRegister";
}

//===----------------------------------------------------------------------===//
// Immediate Operand Definition
//===----------------------------------------------------------------------===//
def uimm4 : Operand<i16>,
    ImmLeaf<i16, [{return isInt<4>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<4>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<4>";
}

def simm13 : Operand<i16>,
    ImmLeaf<i16, [{return isInt<13>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<13>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<13>";
}

def uimm3_plus1 : Operand<i16>,
    ImmLeaf<i16, [{return isUInt<3>(Imm - 1);}]> {
  let ParserMatchClass = UImmAsmOperand<3, "Sub1">;
  let EncoderMethod = "getImmOpValueSub1";
  let DecoderMethod = "decodeUImmOperandAndPlus1<3>";
}

//Load Relative Upper Immediate
class UImm20Operand<ValueType VT> : Operand<VT>,
    ImmLeaf<VT, [{return isInt<20>(Imm);}]> {
  let EncoderMethod = "getImmOpValue";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<20>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def uimm20 : UImm20Operand<i16> {
  let ParserMatchClass = UImmAsmOperand<16, "LI">;
  let DecoderMethod = "decodeUImmOperand<20>";
}
def uimm20_pcaddi : UImm20Operand<XLenVT> {
  let ParserMatchClass = UImmAsmOperand<20, "LA">;
  let DecoderMethod = "decodeUImmOperand<20>";
}

// A 26-bit signed immediate where the least significant bit is zero.
def SImm26Lsb0BranchAsmOperand : SImmAsmOperand<26, "Lsb0"> {
  let ParserMethod = "parseImmediate";
}

def simm25_jump : Operand<OtherVT> {
  let ParserMatchClass = SImm26Lsb0BranchAsmOperand;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<25>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<25, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 21-bit signed immediate where the least significant bit is zero.
def simm20_brcc : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<21, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 21-bit signed immediate where the least significant bit is zero.
def simm20_brind : Operand<XLenVT>,
    ImmLeaf<XLenVT, [{return isInt<21>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<21, "Lsb0">;
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def simm15_hint : Operand<i16>,
    ImmLeaf<i16, [{return isInt<16>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<16, "Lsb0">;
  let EncoderMethod = "getImmOpValueAsr<1>";
  let DecoderMethod = "decodeSImmOperandAndLsl1<15>";
}

def CallSymbol : AsmOperandClass {
  let Name = "CallSymbol";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseCallSymbol";
}

def BareSymbol : AsmOperandClass {
  let Name = "BareSymbol";
  let RenderMethod = "addImmOperands";
  let ParserMethod = "parseBareSymbol";
}

// A bare symbol used in call/tail only.
def call_symbol : Operand<XLenVT> {
  let ParserMatchClass = CallSymbol;
}

// A bare symbol used in "PseudoLA" instructions.
def bare_symbol : Operand<XLenVT> {
  let ParserMatchClass = BareSymbol;
}

def csr_sysreg : Operand<i16> {
  let ParserMatchClass = CSRSystemRegister;
  let PrintMethod = "printCSRSystemRegister";
  let DecoderMethod = "decodeUImmOperand<13>";
}

//===----------------------------------------------------------------------===//
// Instruction Class Templates
//===----------------------------------------------------------------------===//
class Load_ri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs IntRegs:$rs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteLoad, ReadMemBase]>;
class Load_di<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs IntPair:$rs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[WriteLoad, ReadMemBase]>;

class Store_rri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs), (ins IntRegs:$rs, MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[ReadStoreData, ReadMemBase]>;
class Store_dri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs), (ins IntPair:$rs, MemRegs:$rb, simm13:$imm13),
              opcodestr, "$rs, ${imm13}(${rb})">,
      Sched<[ReadStoreData, ReadMemBase]>;

class Cache_ri<bits<3> func3, bits<2> tag2, string opcodestr>
    : FMT_LS<func3, tag2, (outs), (ins MemRegs:$rb, simm13:$imm13),
              opcodestr, "${imm13}(${rb})">,
      Sched<[ReadMemBase]> {
  let rs = 0;
}

class ALU_r<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntRegs:$rd), (ins IntRegs:$rs1),
              opcodestr, "$rd, $rs1">,
      Sched<[WriteIALU, ReadIALU]> {
  let rs2 = 0;
}

class ALU_ri<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_I<func3, tag3, (outs IntRegs:$rd), (ins IntRegs:$rs, simm13:$imm13),
              opcodestr, "$rd, $rs, $imm13">,
      Sched<[WriteIALU, ReadIALU]>;

class BFM_rii<bits<3> func3, bits<3> tag3, bits<1> ifsigned, string opcodestr>
    : FMT_II<func3, tag3, ifsigned, (outs IntRegs:$rd), (ins IntRegs:$rs, uimm4:$lsb, uimm4:$len),
              opcodestr, "$rd, $rs, $lsb, $len">,
      Sched<[WriteIALU, ReadIALU]>;

class ALU_rr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntRegs:$rd), (ins IntRegs:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALU_rri<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_RRI<func3, tag3, (outs IntRegs:$rd), (ins IntRegs:$rs1, IntRegs:$rs2, uimm4:$flag7),
              opcodestr, "$rd, $rs1, $rs2, $flag7">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class SHADD_rri<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_RRI<func3, tag3, (outs IntRegs:$rd), (ins IntRegs:$rs1, IntRegs:$rs2, uimm3_plus1:$flag7),
              opcodestr, "$rd, $rs1, $rs2, $flag7">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALUd_rr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntPair:$rd), (ins IntRegs:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALUd_dr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntPair:$rd), (ins IntPair:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

class ALUd_di<bits<3> func3, bits<3> tag3, string opcodestr>
    : FMT_I<func3, tag3, (outs IntPair:$rd), (ins IntPair:$rs, simm13:$imm13),
              opcodestr, "$rd, $rs, $imm13">,
      Sched<[WriteIALU, ReadIALU]>;

class CMOV_rr<bits<3> func3, bits<3> tag3, bits<7> flag7, string opcodestr>
    : FMT_R<func3, tag3, flag7, (outs IntRegs:$rd), (ins IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]> {
  let Constraints = "$T = $rd";
}

class CondB_ri<bits<3> func3, string opcodestr>
    : FMT_B<func3, (outs), (ins IntRegs:$rs, simm20_brcc:$imm20),
              opcodestr, "$rs, $imm20">,
      Sched<[WriteBranch, ReadBranch]> {
  let isBranch = 1;
  let isTerminator = 1;
}

class HINT<bits<3> func3, bits<5> tag5, string opcodestr>
    : FMT_HT<func3, tag5, (outs), (ins), opcodestr, ""> {
  let rb = 0;
  let imm15 = 0;
}

class HINT_ri<bits<3> func3, bits<5> tag5, string opcodestr>
    : FMT_HT<func3, tag5, (outs), (ins MemRegs:$rb, simm15_hint:$imm15),
              opcodestr, "${imm15}(${rb})">,
      Sched<[ReadMemBase]>;

//===----------------------------------------------------------------------===//
// Instructions common
//===----------------------------------------------------------------------===//
//Load Store
let IsSignExt = 1 in {
def LW : Load_ri<0b001, 0b00, "lw">;
def LB : Load_ri<0b001, 0b10, "lb">;
def LBU : Load_ri<0b001, 0b11, "lbu">;
}
def SW : Store_rri<0b000, 0b00, "sw">;
def SB : Store_rri<0b000, 0b10, "sb">;
let IsSignExt = 1 in {
def LD : Load_di<0b001, 0b01, "ld">;
}
def SD : Store_dri<0b000, 0b01, "sd">;

def DCZERO  : Cache_ri<0b100, 0b01, "dc.zero">;
def DCINVD  : Cache_ri<0b110, 0b01, "dc.invd">;
def DCFLUSH : Cache_ri<0b101, 0b01, "dc.flush">;
def DCCLEAN : Cache_ri<0b111, 0b01, "dc.clean">;

//Load Relative Address
let IsSignExt = 1 in {
def LI : FMT_LRA<0b000, (outs IntRegs:$rd), (ins uimm20:$imm20),
    "li", "$rd, $imm20">, Sched<[WriteIALU]>;
def PCADDI : FMT_LRA<0b100, (outs IntRegs:$rd), (ins uimm20_pcaddi:$imm20),
    "pcaddi", "$rd, $imm20">, Sched<[WriteIALU]>;
def PCADD12I : FMT_LRA<0b101, (outs IntRegs:$rd), (ins uimm20_pcaddi:$imm20),
    "pcadd12i", "$rd, $imm20">, Sched<[WriteIALU]>;
def PCADD20I : FMT_LRA<0b110, (outs IntRegs:$rd), (ins uimm20_pcaddi:$imm20),
    "pcadd20i", "$rd, $imm20">, Sched<[WriteIALU]>;
}

//Read CSR
let hasNoSchedulingInfo = 1,
    hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
class CSRRr_ri<bits<2> tag2, string opcodestr, RegisterClass RC>
  : FMT_SR<0b100, tag2, (outs RC:$rs), (ins csr_sysreg:$imm13),
              opcodestr, "$imm13, $rs">,
      Sched<[ReadCSR]> {
  let pos5 = 0;
}

//Write CSR
class CSRW_rri<bits<2> tag2, string opcodestr, RegisterClass RC>
  : FMT_SR<0b010, tag2, (outs), (ins csr_sysreg:$imm13, RC:$rs),
              opcodestr, "$imm13, $rs">,
      Sched<[WriteCSR]> {
  let pos5 = 0;
}

//Atomic Exchange CSR
class CSRXr_rri<bits<2> tag2, string opcodestr, RegisterClass RC>
  : FMT_SR<0b000, tag2, (outs RC:$T), (ins csr_sysreg:$imm13, RC:$rs),
              opcodestr, "$imm13, $rs">,
      Sched<[ReadCSR, WriteCSR]> {
  let pos5 = 0;
  let Constraints = "$T = $rs";
}

//Notify CSR
class CSRNT_ri<bits<2> tag2, string opcodestr>
  : FMT_SR<0b110, tag2, (outs), (ins csr_sysreg:$imm13),
              opcodestr, "$imm13"> {
  let pos5 = 0;
  let rs = 0;
}
}

def RCSR : CSRRr_ri<0b01, "rcsr", IntRegs>;
def RCSRH : CSRRr_ri<0b10, "rcsr.h", IntRegs>;
def RCSRD : CSRRr_ri<0b11, "rcsr.d", IntPair>;

def WCSR : CSRW_rri<0b01, "wcsr", IntRegs>;
def WCSRH : CSRW_rri<0b10, "wcsr.h", IntRegs>;
def WCSRD : CSRW_rri<0b11, "wcsr.d", IntPair>;

def XCSR : CSRXr_rri<0b01, "xcsr", IntRegs>;
def XCSRH : CSRXr_rri<0b10, "xcsr.h", IntRegs>;
def XCSRD : CSRXr_rri<0b11, "xcsr.d", IntPair>;

def NTCSR : CSRNT_ri<0b01, "ntcsr">;
def NTCSRH : CSRNT_ri<0b10, "ntcsr.h">;
def NTCSRD : CSRNT_ri<0b11, "ntcsr.d">;

//Arith
def ADD : ALU_rr<0b000, 0b000, 0, "add">;
def SUB : ALU_rr<0b000, 0b011, 0, "sub">;
let IsSignExt = 1 in {
def SLT : ALU_rr<0b000, 0b100, 0, "slt">;
def SLTU : ALU_rr<0b000, 0b110, 0, "sltu">;
}
def ADDI : ALU_ri<0b000, 0b000, "add.i">;
let IsSignExt = 1 in {
  def SLTI : ALU_ri<0b000, 0b100, "slt.i">;
  def SLTUI : ALU_ri<0b000, 0b110, "sltu.i">;
}
def ADD32 : ALUd_dr<0b000, 0b010, 0, "add32">;
def SUB32 : ALUd_dr<0b000, 0b001, 0, "sub32">;
def ADDI32 : ALUd_di<0b000, 0b010, "add32.i">;
def MOVZ : CMOV_rr<0b000, 0b101, 0, "movz">;
def MOVN : CMOV_rr<0b000, 0b111, 0, "movn">;

//Shift
def SLL : ALU_rr<0b010, 0b000, 0, "sll">;
def SRL : ALU_rr<0b010, 0b010, 0, "srl">;
def SRA : ALU_rr<0b010, 0b100, 0, "sra">;
def SRR : ALU_rr<0b010, 0b110, 0, "srr">;
def SLLI : ALU_ri<0b010, 0b000, "sll.i">;
def SRLI : ALU_ri<0b010, 0b010, "srl.i">;
def SRAI : ALU_ri<0b010, 0b100, "sra.i">;
def SRRI : ALU_ri<0b010, 0b110, "srr.i">;
def SHADD : SHADD_rri<0b010, 0b001, "shadd">;

//Logical
def OR : ALU_rr<0b001, 0b000, 0, "or">;
def AND : ALU_rr<0b001, 0b100, 0, "and">;
def XOR : ALU_rr<0b001, 0b010, 0, "xor">;
def ORI : ALU_ri<0b001, 0b000, "or.i">;
def ANDI : ALU_ri<0b001, 0b100, "and.i">;
def XORI : ALU_ri<0b001, 0b010, "xor.i">;
def ORN : ALU_rr<0b001, 0b001, 0, "orn">;
def ANDN : ALU_rr<0b001, 0b101, 0, "andn">;
def XNOR : ALU_rr<0b001, 0b011, 0, "xnor">;

//Multiply
def MUL : ALU_rr<0b011,     0b000, 0, "mul">;      //multiply
def MULH : ALU_rr<0b011,    0b100, 0, "mulh">;     //multiply high
def MULHU : ALU_rr<0b011,   0b111, 0, "mulh.u">;   //multiply high unsigned
def MULUS : ALU_rr<0b011,   0b110, 0, "mulh.us">;  //multiply high unsigned signed
def MULI : ALU_ri<0b011,    0b000,    "mul.i">;    //multiply with immediate
def MULHI : ALU_ri<0b011,   0b100,    "mulh.i">;   //multiply high with immediate
def MULUSI : ALU_ri<0b011,  0b110,    "mulh.usi">; //multiply high unsigned signed with immediate
def MULD : ALUd_rr<0b011,   0b001, 0, "mulhl">;    //multiply high low
def MULDU : ALUd_rr<0b011,  0b011, 0, "mulhl.u">;  //multiply high low unsigned
def MULDUS : ALUd_rr<0b011, 0b010, 0, "mulhl.us">; //multiply high low unsigned signed

//Bitmanipulation
def REV : ALU_r<0b101, 0b001, 0, "rev">;
def CLZ : ALU_r<0b101, 0b101, 0, "clz">;
def MIN : ALU_rr<0b101, 0b011, 0, "min">;
def MAX : ALU_rr<0b101, 0b011, 2, "max">;
def MINU : ALU_rr<0b101, 0b011, 1, "minu">;
def MAXU : ALU_rr<0b101, 0b011, 3, "maxu">;
def TBE : ALU_rr<0b101, 0b110, 0, "tbe">;
def PACK : ALU_rri<0b101, 0b111, "pack">;
def UBFI : BFM_rii<0b101, 0b000, 0, "ubfi">;
def SBFI : BFM_rii<0b101, 0b000, 1, "sbfi">;
def UBFX : BFM_rii<0b101, 0b010, 0, "ubfx">;
def SBFX : BFM_rii<0b101, 0b010, 1, "sbfx">;

//System Ctl
def NOP : HINT<0b000, 0b00000, "nop">;
def WFI : HINT<0b000, 0b00001, "wfi">;
def WFE : HINT<0b000, 0b00010, "wfe">;
def WFM : HINT_ri<0b000, 0b00011, "wfm">;

def UNIMP  : HINT<0b001, 0b00001, "unimp">,  Sched<[WriteBranch]>;
def EBREAK : HINT<0b001, 0b00010, "ebreak">, Sched<[WriteBranch]>;
def ECALL  : HINT<0b001, 0b00011, "ecall">,  Sched<[WriteBranch]>;

def ADJCALLSTACKUP : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                     "!ADJCALLSTACKUP", "$amt1",
                     [(callseq_end timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i16imm:$amt1, i16imm:$amt2),
                       "!ADJCALLSTACKDOWN", "$amt1, $amt2",
                       [(callseq_start timm:$amt1, timm:$amt2)]>;

def B : FMT_Ji<0b000,   (outs), (ins simm25_jump:$imm25),
  "b", "$imm25", []>,         Sched<[WriteBranch]>;
def BL : FMT_Ji<0b001,  (outs), (ins simm25_jump:$imm25),
  "bl", "$imm25", []>,        Sched<[WriteIALU, WriteBranch]>;
def BR : FMT_Jr<0b100,  (outs), (ins IntRegs:$rb, simm20_brind:$imm20),
  "br", "$rb, $imm20", []>,   Sched<[WriteBranch, ReadBranch]>;
def BLR : FMT_Jr<0b101, (outs), (ins IntRegs:$rb, simm20_brind:$imm20),
  "blr", "$rb, $imm20", []>,  Sched<[WriteIALU, WriteBranch, ReadBranch]>;

let isBarrier = 1, isReturn = 1, isTerminator = 1 in {
def RET : FMT_J<0b010, 0b00010, (outs), (ins),
  "ret", "",  [(sodium_ret)]>,  Sched<[WriteBranch, ReadBranch]>;
def ERET : FMT_J<0b110, 0b00000, (outs), (ins),
  "eret", "", [(sodium_eret)]>, Sched<[WriteBranch, ReadBranch]>;
}

//===----------------------------------------------------------------------===//
// Instructions related to Flags, which have no fixups.
//===----------------------------------------------------------------------===//
let isBarrier = 1, isBranch = 1, isTerminator = 1 in
def PseudoBR : Pseudo<(outs), (ins simm25_jump:$imm25), "", "", [(br bb:$imm25)]>,
                PseudoInstExpansion<(B simm25_jump:$imm25)>;  //jump flag => b offset(flag)

let isBarrier = 1, isBranch = 1, isIndirectBranch = 1, isTerminator = 1 in
def PseudoBRIND : Pseudo<(outs), (ins IntRegs:$rb, simm20_brind:$imm20), "", "", []>,
                PseudoInstExpansion<(BR IntRegs:$rb, simm20_brind:$imm20)>;
                                                              //brind jt  => br $rb(flag), offset(flag)

def CBZ  : CondB_ri<0b001, "bc.ez">;
def CBNZ : CondB_ri<0b101, "bc.nz">;
def CBGE : CondB_ri<0b011, "bc.ge">;
def CBLT : CondB_ri<0b110, "bc.lt">;
def CBGT : CondB_ri<0b010, "bc.gt">;
def CBLE : CondB_ri<0b111, "bc.le">;

//===----------------------------------------------------------------------===//
// Instructions related to Symbols, which need expand and fixup.
//===----------------------------------------------------------------------===//
// Expanded(expand at emit level, and share one fixup) at XXXMCCodeEmitter.cpp
let isBranch = 1, isIndirectBranch = 1, isTerminator = 1, isCodeGenOnly = 0, isBarrier = 1, Size = 8 in
def PseudoJump : Pseudo<(outs), (ins call_symbol:$dst),
                "jmp", "$dst", []>,
                Sched<[WriteBranch, ReadBranch]>;             //brind sym  => br  (pcadd20i %hi(sym)), %lo(sym),  relax to br X0, offset(sym)
                                                              // formed by insertIndirectBranch, expanded by expandFunctionCall
let isCall = 1, isCodeGenOnly = 0, Size = 8 in
def PseudoCall : Pseudo<(outs), (ins call_symbol:$dst),
                "call", "$dst", []>,
                Sched<[WriteIALU, WriteBranch]>;              //call sym   => blr (pcadd20i %hi(sym)), %lo(sym),  relax to bl offset(sym)
let isCall = 1 in
def PseudoCallIndirect : Pseudo<(outs), (ins IntRegs:$rb), "", "",
                [(sodium_call IntRegs:$rb)]>,
                PseudoInstExpansion<(BLR IntRegs:$rb, 0)>;    //call (sym) => blr $rb(sym), 0

let isCall = 1, isReturn = 1, isTerminator = 1, isCodeGenOnly = 0, isBarrier = 1, Size = 8 in
def PseudoTail : Pseudo<(outs), (ins call_symbol:$dst),
                "tail", "$dst", []>,
                 Sched<[WriteIALU, WriteBranch, ReadBranch]>; //tail sym   => blr (pcadd20i %hi(sym)), %lo(sym),  relax to bl offset(sym)

let isCall = 1, isReturn = 1, isTerminator = 1, isBarrier = 1 in
def PseudoTailIndirect : Pseudo<(outs), (ins IntRegs:$rb), "", "",
                [(sodium_tail IntRegs:$rb)]>,
                PseudoInstExpansion<(BLR IntRegs:$rb, 0)>;    //tail (sym) => blr $rb(sym), 0

// Expanded at XXXExpandPseudoInsts.cpp
let isCodeGenOnly = 0, Size = 8 in {
def PseudoLA : Pseudo<(outs IntRegs:$dst), (ins bare_symbol:$src),
                "la", "$dst, $src", []>;                      //la sym     => addi (pcadd12i %hi(sym)), %lo(sym)
}

//===----------------------------------------------------------------------===//
// Codegen patterns
//
// Naming convention: For 'generic' pattern classes, we use the naming
// convention PatTy1Ty2. For pattern classes which offer a more complex
// expansion, prefix the class name, e.g. ccBrPats.
//===----------------------------------------------------------------------===//
//Immediate
def : Pat<(i16 uimm20:$imm20), (LI uimm20:$imm20)>;

//Branch Indirect Pattern
def : Pat<(brind IntRegs:$rb), (PseudoBRIND IntRegs:$rb, 0)>;
def : Pat<(brind (add IntRegs:$rb, simm20_brind:$imm20)),
  (PseudoBRIND IntRegs:$rb, simm20_brind:$imm20)>;

//Call & Tail Pattern
def : Pat<(sodium_call tglobaladdr:$func), (PseudoCall tglobaladdr:$func)>;
def : Pat<(sodium_call texternalsym:$func), (PseudoCall texternalsym:$func)>;
def : Pat<(sodium_tail (iPTR tglobaladdr:$dst)), (PseudoTail tglobaladdr:$dst)>;
def : Pat<(sodium_tail (iPTR texternalsym:$dst)), (PseudoTail texternalsym:$dst)>;

//Load Symbol
def : Pat<(sodium_li tjumptable:$in), (LI tjumptable:$in)>;
def : Pat<(sodium_li tconstpool:$in), (LI tconstpool:$in)>;
def : Pat<(sodium_li tglobaladdr:$in), (LI tglobaladdr:$in)>;
def : Pat<(sodium_li tblockaddress:$in), (LI tblockaddress:$in)>;

def : Pat<(sodium_la tjumptable:$in), (PseudoLA tjumptable:$in)>;
def : Pat<(sodium_la tconstpool:$in), (PseudoLA tconstpool:$in)>;
def : Pat<(sodium_la tglobaladdr:$in), (PseudoLA tglobaladdr:$in)>;
def : Pat<(sodium_la tblockaddress:$in), (PseudoLA tblockaddress:$in)>;

//Cond Move
def : Pat<(select IntRegs:$rs1, IntRegs:$T, IntRegs:$rs2),
  (MOVZ IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(select IntRegs:$rs1, IntRegs:$rs2, IntRegs:$T),
  (MOVN IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;

def : Pat<(select (i16 (seteq IntRegs:$rs1, 0)), IntRegs:$T, IntRegs:$rs2),
  (MOVN IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(select (i16 (seteq IntRegs:$rs1, 0)), IntRegs:$rs2, IntRegs:$T),
  (MOVZ IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;

def : Pat<(select (i16 (setne IntRegs:$rs1, 0)), IntRegs:$T, IntRegs:$rs2),
  (MOVZ IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(select (i16 (setne IntRegs:$rs1, 0)), IntRegs:$rs2, IntRegs:$T),
  (MOVN IntRegs:$T, IntRegs:$rs1, IntRegs:$rs2)>;

def : Pat<(select (i16 (seteq IntRegs:$rs1, 0)), 0, IntRegs:$T),
  (MOVZ IntRegs:$T, IntRegs:$rs1, X0)>;
def : Pat<(select (i16 (setne IntRegs:$rs1, 0)), 0, IntRegs:$T),
  (MOVN IntRegs:$T, IntRegs:$rs1, X0)>;

multiclass ccMvPats<PatFrag CondOp, SodiumInst CondInst, SodiumInst A, SodiumInst B> {
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$rs3, IntRegs:$T),
  (A IntRegs:$T, (CondInst IntRegs:$rs1, IntRegs:$rs2), IntRegs:$rs3)>;
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$T, IntRegs:$rs3),
  (B IntRegs:$T, (CondInst IntRegs:$rs1, IntRegs:$rs2), IntRegs:$rs3)>;
}
multiclass ccMvPatsR<PatFrag CondOp, SodiumInst CondInst, SodiumInst A, SodiumInst B> {
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$rs3, IntRegs:$T),
  (A IntRegs:$T, (CondInst IntRegs:$rs2, IntRegs:$rs1), IntRegs:$rs3)>;
def : Pat<(select (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), IntRegs:$T, IntRegs:$rs3),
  (B IntRegs:$T, (CondInst IntRegs:$rs2, IntRegs:$rs1), IntRegs:$rs3)>;
}

defm : ccMvPats <seteq,  XOR,  MOVZ, MOVN>;
defm : ccMvPats <setne,  XOR,  MOVN, MOVZ>;

defm : ccMvPats <setge,  SLT,  MOVZ, MOVN>;
defm : ccMvPats <setuge, SLTU, MOVZ, MOVN>;
defm : ccMvPats <setlt,  SLT,  MOVN, MOVZ>;
defm : ccMvPats <setult, SLTU, MOVN, MOVZ>;

defm : ccMvPatsR<setle,  SLT,  MOVZ, MOVN>;
defm : ccMvPatsR<setule, SLTU, MOVZ, MOVN>;
defm : ccMvPatsR<setgt,  SLT,  MOVN, MOVZ>;
defm : ccMvPatsR<setugt, SLTU, MOVN, MOVZ>;

//Cond Branch
def : Pat<(brcond IntRegs:$rs, bb:$dst), (CBNZ IntRegs:$rs, simm20_brcc:$dst)>;

def : Pat<(brcond (i16 (seteq IntRegs:$rs,  0)), bb:$dst), (CBZ  IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setne IntRegs:$rs,  0)), bb:$dst), (CBNZ IntRegs:$rs, simm20_brcc:$dst)>;

def : Pat<(brcond (i16 (setgt IntRegs:$rs,  0)), bb:$dst), (CBGT IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setle IntRegs:$rs,  0)), bb:$dst), (CBLE IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setge IntRegs:$rs,  0)), bb:$dst), (CBGE IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setlt IntRegs:$rs,  0)), bb:$dst), (CBLT IntRegs:$rs, simm20_brcc:$dst)>;

def : Pat<(brcond (i16 (setgt IntRegs:$rs, -1)), bb:$dst), (CBGE IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setle IntRegs:$rs, -1)), bb:$dst), (CBLT IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setge IntRegs:$rs,  1)), bb:$dst), (CBGT IntRegs:$rs, simm20_brcc:$dst)>;
def : Pat<(brcond (i16 (setlt IntRegs:$rs,  1)), bb:$dst), (CBLE IntRegs:$rs, simm20_brcc:$dst)>;

class ccBrPats <PatFrag CondOp, SodiumInst CondInst, SodiumInst BrInst>
  : Pat<(brcond (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), bb:$dst),
    (BrInst (CondInst IntRegs:$rs1, IntRegs:$rs2), simm20_brcc:$dst)>;
class ccBrPatsR<PatFrag CondOp, SodiumInst CondInst, SodiumInst BrInst>
  : Pat<(brcond (i16 (CondOp IntRegs:$rs1, IntRegs:$rs2)), bb:$dst),
    (BrInst (CondInst IntRegs:$rs2, IntRegs:$rs1), simm20_brcc:$dst)>;

def : ccBrPats <seteq,  XOR,  CBZ>;
def : ccBrPats <setne,  XOR,  CBNZ>;

def : ccBrPats <setge,  SLT,  CBZ>;
def : ccBrPats <setuge, SLTU, CBZ>;
def : ccBrPats <setlt,  SLT,  CBNZ>;
def : ccBrPats <setult, SLTU, CBNZ>;

def : ccBrPatsR<setle,  SLT,  CBZ>;
def : ccBrPatsR<setule, SLTU, CBZ>;
def : ccBrPatsR<setgt,  SLT,  CBNZ>;
def : ccBrPatsR<setugt, SLTU, CBNZ>;

//Cond Set
def : Pat<(seteq IntRegs:$rs, 0),  (SLTUI IntRegs:$rs, 1)>;
def : Pat<(setne IntRegs:$rs, 0),  (SLTU X0, IntRegs:$rs)>;
def : Pat<(setne IntRegs:$rs, -1), (SLTUI IntRegs:$rs, -1)>;

def : Pat<(seteq IntRegs:$rs1, IntRegs:$rs2), (SLTUI (XOR IntRegs:$rs1, IntRegs:$rs2), 1)>;
def : Pat<(setne IntRegs:$rs1, IntRegs:$rs2), (SLTU X0, (XOR IntRegs:$rs1, IntRegs:$rs2))>;
def : Pat<(setle IntRegs:$rs1, IntRegs:$rs2), (XORI (SLT  IntRegs:$rs2, IntRegs:$rs1), 1)>;
def : Pat<(setule IntRegs:$rs1, IntRegs:$rs2),(XORI (SLTU IntRegs:$rs2, IntRegs:$rs1), 1)>;
def : Pat<(setgt IntRegs:$rs1, IntRegs:$rs2), (SLT  IntRegs:$rs2, IntRegs:$rs1)>;
def : Pat<(setugt IntRegs:$rs1, IntRegs:$rs2),(SLTU IntRegs:$rs2, IntRegs:$rs1)>;
def : Pat<(setge IntRegs:$rs1, IntRegs:$rs2), (XORI (SLT IntRegs:$rs1, IntRegs:$rs2), 1)>;
def : Pat<(setuge IntRegs:$rs1, IntRegs:$rs2),(XORI (SLTU IntRegs:$rs1, IntRegs:$rs2), 1)>;

//Generic pattern classes
class PatReg<SDPatternOperator OpNode, SodiumInst Inst>
    : Pat<(i16 (OpNode (i16 IntRegs:$rs))), (Inst IntRegs:$rs)>;
class PatRegReg<SDPatternOperator OpNode, SodiumInst Inst>
    : Pat<(i16 (OpNode (i16 IntRegs:$rs1), (i16 IntRegs:$rs2))), (Inst IntRegs:$rs1, IntRegs:$rs2)>;
class PatRegImm<SDPatternOperator OpNode, SodiumInst Inst, ImmLeaf ImmType>
    : Pat<(i16 (OpNode (i16 IntRegs:$rs), ImmType:$imm)),
          (Inst IntRegs:$rs, ImmType:$imm)>;
class PatRegSimm13<SDPatternOperator OpNode, SodiumInst Inst>
    : PatRegImm<OpNode, Inst, simm13>;

//Arith
def : PatRegReg<add, ADD>;
def : PatRegSimm13<add, ADDI>;
def : PatRegReg<sub, SUB>;
def : PatRegReg<setlt, SLT>;
def : PatRegReg<setult, SLTU>;
def : PatRegSimm13<setlt, SLTI>;
def : PatRegSimm13<setult, SLTUI>;
def : PatRegReg<or, OR>;
def : PatRegSimm13<or, ORI>;
def : PatRegReg<and, AND>;
def : PatRegSimm13<and, ANDI>;
def : PatRegReg<xor, XOR>;
def : PatRegSimm13<xor, XORI>;
def : Pat<(not IntRegs:$rs), (XORI IntRegs:$rs, -1)>;
def : PatRegReg<shl, SLL>;
def : PatRegSimm13<shl, SLLI>;
def : PatRegReg<srl, SRL>;
def : PatRegSimm13<srl, SRLI>;
def : PatRegReg<sra, SRA>;
def : PatRegSimm13<sra, SRAI>;
def : PatRegReg<rotr, SRR>;
def : PatRegSimm13<rotr, SRRI>;

def ImmSubFrom16 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(16 - N->getZExtValue(), SDLoc(N),
                                   N->getValueType(0));
}]>;
def : Pat<(i16 (rotl IntRegs:$rs1, IntRegs:$rs2)),  (SRR IntRegs:$rs1, (SUB X0, IntRegs:$rs2))>;
def : Pat<(i16 (rotl IntRegs:$rs1, simm13:$imm13)), (SRRI IntRegs:$rs1, (ImmSubFrom16 simm13:$imm13))>;

def : Pat<(i16 (add (shl (i16 IntRegs:$rs1), uimm3_plus1:$flag7), (i16 IntRegs:$rs2))),
  (SHADD IntRegs:$rs1, IntRegs:$rs2, uimm3_plus1:$flag7)>;

//Muplication
def : PatRegReg<mul, MUL>;
def : PatRegReg<mulhs, MULH>;
def : PatRegReg<mulhu, MULHU>;
def : PatRegSimm13<mul, MULI>;
def : PatRegSimm13<mul, MULHI>;

//Bit Manipulation
def : PatReg<ctlz, CLZ>;
def : PatReg<bitreverse, REV>;
def : PatRegReg<smin, MIN>;
def : PatRegReg<smax, MAX>;
def : PatRegReg<umin, MINU>;
def : PatRegReg<umax, MAXU>;
def : Pat<(i16 (cttz (i16 IntRegs:$rs))),  (CLZ (REV IntRegs:$rs))>;
def : Pat<(i16 (bswap (i16 IntRegs:$rs))), (SRRI IntRegs:$rs, 8)>;
def : PatRegReg<sodium_tbe, TBE>;

def zexti8 : ComplexPattern<i16, 1, "SelectZExtBits<8>">;
def : Pat<(i16 (or (shl IntRegs:$rs1, (i16 8)), (zexti8 (i16 IntRegs:$rs2)))),
  (PACK IntRegs:$rs1, IntRegs:$rs2, 8)>;
def : Pat<(i16 (sext_inreg IntRegs:$rs1, i1)), (SBFX IntRegs:$rs1, 0, 0)>;
def : Pat<(i16 (sext_inreg IntRegs:$rs1, i8)), (SBFX IntRegs:$rs1, 0, 7)>;

def : Pat<(i16 (and IntRegs:$rs1, (not IntRegs:$rs2))), (ANDN IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(i16 (or  IntRegs:$rs1, (not IntRegs:$rs2))), (ORN  IntRegs:$rs1, IntRegs:$rs2)>;
def : Pat<(i16 (xor IntRegs:$rs1, (not IntRegs:$rs2))), (XNOR IntRegs:$rs1, IntRegs:$rs2)>;

// intrinsic ref:
//  llvm-project/llvm/include/llvm/IR/IntrinsicsSodium.td
//  llvm-project/clang/include/clang/Basic/BuiltinsSodium.def
def : Pat<(i16 (int_sodium_pack IntRegs:$rs1, IntRegs:$rs2, uimm4:$imm4)), (PACK IntRegs:$rs1, IntRegs:$rs2, uimm4:$imm4)>;

//===----------------------------------------------------------------------===//
// v2i16 codegen patterns
//===----------------------------------------------------------------------===//
// build_vector
def : Pat<(v2i16 (build_vector (i16 0), (i16 0))),
          (COPY D0)>;
def : Pat<(v2i16 (build_vector (i16 IntRegs:$rs1), (i16 IntRegs:$rs2))),
          (REG_SEQUENCE IntPair,
                        (i16 IntRegs:$rs1), sub_even,
                        (i16 IntRegs:$rs2), sub_odd)>;
/*
def : Pat<(build_vector (i16 IntRegs:$rs1), (i16 IntRegs:$rs2)),
          (INSERT_SUBREG (INSERT_SUBREG (v2i16 (IMPLICIT_DEF)),
            (i16 IntRegs:$rs1), sub_even),
            (i16 IntRegs:$rs2), sub_odd)>;
*/

// extract_vector
def : Pat<(extractelt (v2i16 IntPair:$rs), 0),
          (i16 (EXTRACT_SUBREG IntPair:$rs, sub_even))>;
def : Pat<(extractelt (v2i16 IntPair:$rs), 1),
          (i16 (EXTRACT_SUBREG IntPair:$rs, sub_odd))>;

// smul_lohi, umul_lohi
// use Intrinsic to deal with v2i16 <= i16 x i16
class DPatRegReg<Intrinsic OpNode, SodiumInst Inst>
  : Pat<(v2i16 (OpNode IntRegs:$rs1, IntRegs:$rs2)), (Inst IntRegs:$rs1, IntRegs:$rs2)>;

def : DPatRegReg<int_sodium_mul32, MULD>;
def : DPatRegReg<int_sodium_mulu32, MULDU>;

//===----------------------------------------------------------------------===//
// Special codegen patterns
//===----------------------------------------------------------------------===//
def : Pat<(debugtrap), (EBREAK)>;

// Addressing modes. ref: SodiumComplexPattern.h
def BaseAddr : ComplexPattern<iPTR, 1, "SelectBaseAddr">;
def AddrConstant : ComplexPattern<iPTR, 2, "SelectAddrConstant">;

def AddLike: PatFrags<(ops node:$A, node:$B),
                      [(add node:$A, node:$B), (or node:$A, node:$B)], [{
    return N->getOpcode() == ISD::ADD || isOrEquivalentToAdd(N);
}]>;

def : Pat<(AddLike (i16 BaseAddr:$rb), simm13:$imm13),
          (ADDI (i16 BaseAddr:$rb), simm13:$imm13)>;

multiclass LdPat<PatFrag LoadOp, SodiumInst Inst, ValueType Vt> {
  def : Pat<(Vt (LoadOp BaseAddr:$rb)), (Inst BaseAddr:$rb, 0)>;
  def : Pat<(Vt (LoadOp (AddrConstant IntRegs:$rb, simm13:$imm13))),
  (Inst IntRegs:$rb, simm13:$imm13)>;
  def : Pat<(Vt (LoadOp (AddLike BaseAddr:$rb, simm13:$imm13))),
  (Inst BaseAddr:$rb, simm13:$imm13)>;
}

multiclass StPat<PatFrag StoreOp, SodiumInst Inst, RegisterClass RC, ValueType Vt> {
  def : Pat<(StoreOp (Vt RC:$rd), BaseAddr:$rb),
  (Inst RC:$rd, BaseAddr:$rb, 0)>;
  def : Pat<(StoreOp (Vt RC:$rd), (AddrConstant IntRegs:$rb, simm13:$imm13)),
  (Inst RC:$rd, IntRegs:$rb, simm13:$imm13)>;
  def : Pat<(StoreOp (Vt RC:$rd), (AddLike BaseAddr:$rb, simm13:$imm13)),
  (Inst RC:$rd, BaseAddr:$rb, simm13:$imm13)>;
}

defm : LdPat<load, LW, i16>;
defm : LdPat<sextloadi8, LB, i16>;
defm : LdPat<extloadi8, LBU, i16>;
defm : LdPat<zextloadi8, LBU, i16>;
defm : StPat<store, SW, IntRegs, i16>;
defm : StPat<truncstorei8, SB, IntRegs, i16>;

// load store pairs
defm : LdPat<load, LD, v2i16>;
defm : StPat<store, SD, IntPair, v2i16>;
