bool SodiumAsmParser::parseDirectiveOption() {
  MCAsmParser &Parser = getParser();
  // Get the option token.
  AsmToken Tok = Parser.getTok();
  // At the moment only identifiers are supported.
  if (parseToken(AsmToken::Identifier, "expected identifier"))
    return true;

  //StringRef Option = Tok.getIdentifier();

  // Unknown option.
  Warning(Parser.getTok().getLoc(), "unknown option");
  Parser.eatToEndOfStatement();
  return false;
}

bool SodiumAsmParser::parseDirectiveAttribute() {
  return false;
}
