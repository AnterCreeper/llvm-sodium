//===-- SodiumDisassembler.cpp - Disassembler for Sodium ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the SodiumDisassembler class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/SodiumBaseInfo.h"
#include "MCTargetDesc/SodiumMCTargetDesc.h"
#include "TargetInfo/SodiumTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Endian.h"

using namespace llvm;

#define DEBUG_TYPE "sodium-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class SodiumDisassembler : public MCDisassembler {
  std::unique_ptr<MCInstrInfo const> const MCII;

public:
  SodiumDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx,
                     MCInstrInfo const *MCII)
      : MCDisassembler(STI, Ctx), MCII(MCII) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;

private:
};
} // end anonymous namespace

static MCDisassembler *createSodiumDisassembler(const Target &T,
                                                const MCSubtargetInfo &STI,
                                                MCContext &Ctx) {
  return new SodiumDisassembler(STI, Ctx, T.createMCInstrInfo());
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSodiumDisassembler() {
  // Register the disassembler for each target.
  TargetRegistry::RegisterMCDisassembler(getTheSodium16Target(),
                                         createSodiumDisassembler);
  TargetRegistry::RegisterMCDisassembler(getTheSodium32Target(),
                                         createSodiumDisassembler);
}

static DecodeStatus DecodeIntRegsRegisterClass(MCInst &Inst, uint32_t RegNo,
                                               uint64_t Address,
                                               const MCDisassembler *Decoder) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  MCRegister Reg = Sodium::X0 + RegNo;
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

static DecodeStatus DecodeIntPairRegisterClass(MCInst &Inst, uint32_t RegNo,
                                               uint64_t Address,
                                               const MCDisassembler *Decoder) {
  if (RegNo > 31)
    return MCDisassembler::Fail;
  MCRegister Reg = Sodium::D0 + (RegNo >> 1);
  Inst.addOperand(MCOperand::createReg(Reg));
  return (RegNo & 1) ? MCDisassembler::SoftFail : MCDisassembler::Success;
}

// Match with SodiumInstrInfo.td, then pass to tbgen inc file.
#include "SodiumDecoderMethod.h"
#include "SodiumGenDisassemblerTables.inc"

DecodeStatus SodiumDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                                ArrayRef<uint8_t> Bytes,
                                                uint64_t Address,
                                                raw_ostream &CS) const {
  DecodeStatus Result;
  // It's a 32 bit instruction if bit 0 is 1.
  //if(Bytes[0] & 0x1) {
  //Normal Format - 4Bytes
  Size = 4;
  uint32_t Insn = support::endian::read32le(Bytes.data());

  Result = decodeInstruction(DecoderTableSodium32, MI, Insn, Address, this, STI);
  if(Result != MCDisassembler::Fail) return Result;

  return MCDisassembler::Fail;
  //}

  //Compress Format - 2Bytes
  //Size = 2;
  //uint32_t Insn = support::endian::read16le(Bytes.data());

  //assert(Size == 2 && "Compress Format not supported yet!");
  return MCDisassembler::Fail;
}
