//===- SodiumAsmParser.cpp - Parse Sodium assembly to MCInst instructions -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/SodiumTargetInfo.h"
#include "MCTargetDesc/SodiumAsmBackend.h"
#include "MCTargetDesc/SodiumBaseInfo.h"
#include "MCTargetDesc/SodiumInstPrinter.h"
#include "MCTargetDesc/SodiumMCExpr.h"
#include "MCTargetDesc/SodiumMCTargetDesc.h"
#include "MCTargetDesc/SodiumTargetStreamer.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MathExtras.h"

#include <limits>

using namespace llvm;

#define DEBUG_TYPE "sodium-asm-parser"

namespace {
struct SodiumOperand;
struct ParserOptionsSet {
  bool IsPicEnabled;
};

class SodiumAsmParser : public MCTargetAsmParser {
  ParserOptionsSet ParserOptions;

  SodiumTargetStreamer &getTargetStreamer() {
    assert(getParser().getStreamer().getTargetStreamer() &&
           "do not have a target streamer");
    MCTargetStreamer &TS = *getParser().getStreamer().getTargetStreamer();
    return static_cast<SodiumTargetStreamer &>(TS);
  }

  SMLoc getLoc() const { return getParser().getTok().getLoc(); }
  bool isSodium32() const { return getSTI().hasFeature(Sodium::Feature32Bit); }

  unsigned validateTargetOperandClass(MCParsedAsmOperand &Op,
                                      unsigned Kind) override;
  bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool parseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                     SMLoc &EndLoc) override;
  OperandMatchResultTy tryParseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                                        SMLoc &EndLoc) override;
  ParseStatus parseDirective(AsmToken DirectiveID) override;
  bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  // Helper to actually emit an instruction to the MCStreamer. Also, when
  // possible, compression of the instruction is performed.
  void emitToStreamer(MCStreamer &S, const MCInst &Inst);

  // Check instruction constraints.
  bool validateInstruction(MCInst &Inst, OperandVector &Operands);

  /// Helper for processing MC instructions that have been successfully matched
  /// by MatchAndEmitInstruction. Modifications to the emitted instructions,
  /// like the expansion of pseudo instructions (e.g., "li"), can be performed
  /// in this method.
  bool processInstruction(MCInst &Inst, SMLoc IDLoc, OperandVector &Operands,
                          MCStreamer &Out);
/*
  bool generateImmOutOfRangeError(OperandVector &Operands, uint64_t ErrorInfo,
                                  int64_t Lower, int64_t Upper,
                                  const Twine &Msg);
*/
  bool generateImmOutOfRangeError(SMLoc ErrorLoc, int64_t Lower, int64_t Upper,
                                  const Twine &Msg);

// Auto-generated instruction matching functions
#define GET_ASSEMBLER_HEADER
#include "SodiumGenAsmMatcher.inc"

  bool parseOperand(OperandVector &Operands, StringRef Mnemonic);
  ParseStatus parseRegister(OperandVector &Operands, bool AllowParens = false);
  ParseStatus parseCSRSystemRegister(OperandVector &Operands);
  ParseStatus parseMemOpBaseReg(OperandVector &Operands);
  ParseStatus parseOperandWithModifier(OperandVector &Operands);
  ParseStatus parseImmediate(OperandVector &Operands);
  ParseStatus parseCallSymbol(OperandVector &Operands);
  ParseStatus parseBareSymbol(OperandVector &Operands);

  bool parseDirectiveOption();
  bool parseDirectiveAttribute();

public:
  static bool classifySymbolRef(const MCExpr *Expr,
                                SodiumMCExpr::VariantKind &Kind);

  SodiumAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
                 const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII) {
    MCAsmParserExtension::Initialize(Parser);
    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
};

#include "SodiumAsmOperand.h"

bool SodiumAsmParser::generateImmOutOfRangeError(
    SMLoc ErrorLoc, int64_t Lower, int64_t Upper,
    const Twine &Msg = "immediate must be an integer in the range") {
  return Error(ErrorLoc, Msg + " [" + Twine(Lower) + ", " + Twine(Upper) + "]");
}

/*
bool SodiumAsmParser::generateImmOutOfRangeError(
    OperandVector &Operands, uint64_t ErrorInfo, int64_t Lower, int64_t Upper,
    const Twine &Msg = "immediate must be an integer in the range") {
  SMLoc ErrorLoc = ((SodiumOperand &)*Operands[ErrorInfo]).getStartLoc();
  return generateImmOutOfRangeError(ErrorLoc, Lower, Upper, Msg);
}
*/

#include "SodiumParseMethod.h"

#define GET_REGISTER_MATCHER
#define GET_SUBTARGET_FEATURE_NAME
#define GET_MATCHER_IMPLEMENTATION
#include "SodiumGenAsmMatcher.inc"

// Attempts to match Name as a register (either using the default name or
// alternative ABI names), setting RegNo to the matching register. Upon
// failure, returns a non-valid MCRegister.
static MCRegister matchRegisterNameHelper(StringRef Name) {
  MCRegister Reg = MatchRegisterName(Name);
  if (!Reg)
    Reg = MatchRegisterAltName(Name);
  return Reg;
}

static MCRegister convertIntPairToIntRegs(MCRegister Reg) {
  assert(Reg >= Sodium::D0 && Reg <= Sodium::D15 && "Invalid register");
  return Sodium::X0 + 2 * (Reg - Sodium::D0);
}

unsigned SodiumAsmParser::validateTargetOperandClass(MCParsedAsmOperand &AsmOp,
                                                     unsigned Kind) {
  SodiumOperand &Op = static_cast<SodiumOperand &>(AsmOp);
  if (!Op.isReg())
    return Match_InvalidOperand;

  MCRegister Reg = Op.getReg();
  bool IsRegIntPair = SodiumMCRegisterClasses[Sodium::IntPairRegClassID].contains(Reg);

  // As the parser couldn't differentiate an IntRegs from an IntPair, coerce the
  // register from IntPair to IntRegs if necessary.
  if (IsRegIntPair && Kind == MCK_IntRegs) {
    Op.Reg.RegNum = convertIntPairToIntRegs(Reg);
    return Match_Success;
  }
  return Match_InvalidOperand;
}

ParseStatus SodiumAsmParser::parseRegister(OperandVector &Operands,
                                           bool AllowParens) {
  SMLoc FirstS = getLoc();
  bool HadParens = false;
  AsmToken LParen;

  // If this is an LParen and a parenthesised register name is allowed, parse it
  // atomically.
  if (AllowParens && getLexer().is(AsmToken::LParen)) {
    AsmToken Buf[2];
    size_t ReadCount = getLexer().peekTokens(Buf);
    if (ReadCount == 2 && Buf[1].getKind() == AsmToken::RParen) {
      HadParens = true;
      LParen = getParser().getTok();
      getParser().Lex(); // Eat '('
    }
  }

  switch (getLexer().getKind()) {
  default:
    if (HadParens)
      getLexer().UnLex(LParen);
    return ParseStatus::NoMatch;
  case AsmToken::Identifier:
    StringRef Name = getLexer().getTok().getIdentifier();
    MCRegister RegNo = matchRegisterNameHelper(Name);

    if (!RegNo) {
      if (HadParens)
        getLexer().UnLex(LParen);
      return ParseStatus::NoMatch;
    }
    if (HadParens)
      Operands.push_back(SodiumOperand::createToken("(", FirstS));
    SMLoc S = getLoc();
    SMLoc E = SMLoc::getFromPointer(S.getPointer() + Name.size());
    getLexer().Lex();
    Operands.push_back(SodiumOperand::createReg(RegNo, S, E));
  }

  if (HadParens) {
    getParser().Lex(); // Eat ')'
    Operands.push_back(SodiumOperand::createToken(")", getLoc()));
  }

  return ParseStatus::Success;
}

bool SodiumAsmParser::MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                             OperandVector &Operands,
                                             MCStreamer &Out,
                                             uint64_t &ErrorInfo,
                                             bool MatchingInlineAsm) {
  MCInst Inst;
  FeatureBitset MissingFeatures;

  auto Result = MatchInstructionImpl(Operands, Inst, ErrorInfo, MissingFeatures,
                                     MatchingInlineAsm);
  switch (Result) {
  default:
    break;
  case Match_Success:
    if (validateInstruction(Inst, Operands))
      return true;
    return processInstruction(Inst, IDLoc, Operands, Out);
  case Match_MissingFeature: {
    return Error(IDLoc, "instruction requires a CPU feature not currently enabled");
  }
  case Match_MnemonicFail: {
    return Error(IDLoc, "unrecognized instruction mnemonic");
  }
  case Match_InvalidOperand: {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0ULL) {
      if (ErrorInfo >= Operands.size())
        return Error(ErrorLoc, "too few operands for instruction");

      ErrorLoc = ((SodiumOperand &)*Operands[ErrorInfo]).getStartLoc();
      if (ErrorLoc == SMLoc())
        ErrorLoc = IDLoc;
    }
    return Error(ErrorLoc, "invalid operand for instruction");
  }
  }

  // Handle the case when the error message is of specific type
  // other than the generic Match_InvalidOperand, and the
  // corresponding operand is missing.
  if (Result > FIRST_TARGET_MATCH_RESULT_TY) {
    SMLoc ErrorLoc = IDLoc;
    if (ErrorInfo != ~0ULL && ErrorInfo >= Operands.size())
      return Error(ErrorLoc, "too few operands for instruction");
  }

  llvm_unreachable("Unknown match type detected!");
}

bool SodiumAsmParser::parseRegister(MCRegister &RegNo, SMLoc &StartLoc,
                                   SMLoc &EndLoc) {
  if (tryParseRegister(RegNo, StartLoc, EndLoc) != MatchOperand_Success)
    return Error(StartLoc, "invalid register name");
  return false;
}

OperandMatchResultTy SodiumAsmParser::tryParseRegister(MCRegister &RegNo,
                                                      SMLoc &StartLoc,
                                                      SMLoc &EndLoc) {
  const AsmToken &Tok = getParser().getTok();
  StartLoc = Tok.getLoc();
  EndLoc = Tok.getEndLoc();
  StringRef Name = getLexer().getTok().getIdentifier();

  RegNo = matchRegisterNameHelper(Name);
  if (!RegNo)
    return MatchOperand_NoMatch;

  getParser().Lex(); // Eat identifier token.
  return MatchOperand_Success;
}

/// Looks at a token type and creates the relevant operand from this
/// information, adding to Operands. If operand was parsed, returns false, else
/// true.
bool SodiumAsmParser::parseOperand(OperandVector &Operands, StringRef Mnemonic) {
  // Check if the current operand has a custom associated parser, if so, try to
  // custom parse the operand, or fallback to the general approach.
  ParseStatus Result =
      MatchOperandParserImpl(Operands, Mnemonic, /*ParseForAllFeatures=*/true);
  if (Result.isSuccess())
    return false;
  if (Result.isFailure())
    return true;

  // Attempt to parse token as a register.
  if (parseRegister(Operands, true).isSuccess())
    return false;

  // Attempt to parse token as an immediate
  if (parseImmediate(Operands).isSuccess()) {
    // Parse memory base register if present
    if (getLexer().is(AsmToken::LParen))
      return !parseMemOpBaseReg(Operands).isSuccess();
    return false;
  }

  // Finally we have exhausted all options and must declare defeat.
  Error(getLoc(), "unknown operand");
  return true;
}

bool SodiumAsmParser::ParseInstruction(ParseInstructionInfo &Info,
                                       StringRef Name, SMLoc NameLoc,
                                       OperandVector &Operands) {
  // Ensure that if the instruction occurs when relaxation is enabled,
  // relocations are forced for the file. Ideally this would be done when there
  // is enough information to reliably determine if the instruction itself may
  // cause relaxations. Unfortunately instruction processing stage occurs in the
  // same pass as relocation emission, so it's too late to set a 'sticky bit'
  // for the entire file.
  /*
  if (getSTI().hasFeature(Sodium::FeatureRelax)) {
    auto *Assembler = getTargetStreamer().getStreamer().getAssemblerPtr();
    if (Assembler != nullptr) {
      SodiumAsmBackend &MAB =
          static_cast<SodiumAsmBackend &>(Assembler->getBackend());
      MAB.setForceRelocs();
    }
  }
  */

  // First operand is token for instruction
  Operands.push_back(SodiumOperand::createToken(Name, NameLoc));

  // If there are no more operands, then finish
  if (getLexer().is(AsmToken::EndOfStatement)) {
    getParser().Lex(); // Consume the EndOfStatement.
    return false;
  }

  // Parse first operand
  if (parseOperand(Operands, Name))
    return true;

  // Parse until end of statement, consuming commas between operands
  while (parseOptionalToken(AsmToken::Comma)) {
    // Parse next operand
    if (parseOperand(Operands, Name))
      return true;
  }

  if (getParser().parseEOL("unexpected token")) {
    getParser().eatToEndOfStatement();
    return true;
  }
  return false;
}

void SodiumAsmParser::emitToStreamer(MCStreamer &S, const MCInst &Inst) {
  S.emitInstruction(Inst, getSTI());
}

bool SodiumAsmParser::validateInstruction(MCInst &Inst,
                                          OperandVector &Operands) {
  //unsigned Opcode = Inst.getOpcode();
  return false;
}

bool SodiumAsmParser::processInstruction(MCInst &Inst, SMLoc IDLoc,
                                        OperandVector &Operands,
                                        MCStreamer &Out) {
  Inst.setLoc(IDLoc);
  switch (Inst.getOpcode()) {
  default:
    break;
  }
  emitToStreamer(Out, Inst);
  return false;
}

#include "SodiumAsmDirective.h"

ParseStatus SodiumAsmParser::parseDirective(AsmToken DirectiveID) {
  StringRef IDVal = DirectiveID.getString();

  if (IDVal == ".option")
    return parseDirectiveOption();
  if (IDVal == ".attribute")
    return parseDirectiveAttribute();

  return ParseStatus::NoMatch;
}

bool SodiumAsmParser::classifySymbolRef(const MCExpr *Expr,
                                        SodiumMCExpr::VariantKind &Kind) {
  Kind = SodiumMCExpr::VK_SODIUM_None;
  if (const SodiumMCExpr *RE = dyn_cast<SodiumMCExpr>(Expr)) {
    Kind = RE->getKind();
    Expr = RE->getSubExpr();
  }
  MCValue Res;
  MCFixup Fixup;
  if (Expr->evaluateAsRelocatable(Res, nullptr, &Fixup))
    return Res.getRefKind() == SodiumMCExpr::VK_SODIUM_None;
  return false;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeSodiumAsmParser() {
  RegisterMCAsmParser<SodiumAsmParser> X(getTheSodium16Target());
  RegisterMCAsmParser<SodiumAsmParser> Y(getTheSodium32Target());
}
