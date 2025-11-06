static uint64_t getReloc(uint8_t *loc, const Relocation &rel, uint64_t val) {
  uint64_t insn = read32le(loc);
  switch (rel.type) {
  default:
    llvm_unreachable("unknown relocation");
  case R_SODIUM_BR20: {
    checkInt(loc, val, 21, rel);
    checkAlignment(loc, val, 2, rel);
    return (insn & GENMASK(0, 3))       | //3:0
           (GETBITS(val, 1,  5)  << 4)  | //8:4
           (insn & GENMASK(9, 11))      | //11:9
           (GETBITS(val, 16, 20) << 12) | //16:12
           (insn & GENMASK(17, 21))     | //21:17
           (GETBITS(val, 6,  15) << 22);  //31:22
  }
  case R_SODIUM_BR25: {
    checkInt(loc, val, 26, rel);
    checkAlignment(loc, val, 2, rel);
    return (insn & GENMASK(0, 3))       | //3:0
           (GETBITS(val, 1,  5)  << 4)  | //8:4
           (insn & GENMASK(9, 11))      | //11:9
           (GETBITS(val, 16, 25) << 12) | //21:12
           (GETBITS(val, 6,  15) << 22);  //31:22
  }
  case R_SODIUM_HI19:
  case R_SODIUM_PCREL_HI19: {
    // Add 1 to test if bit 12 is 1, to compensate for low 13 bits being negative.
    val = (val + 0x1000) >> 13;
    checkInt(loc, val, 19, rel);
    return (insn & GENMASK(0, 11))      | //11:0
           (GETBITS(val, 15, 19) << 12) | //16:12
           (GETBITS(val, 0,  14) << 17);  //31:17
  }
  case R_SODIUM_LO13:
  case R_SODIUM_PCREL_LO13: {
    return (insn & GENMASK(0, 16))      | //16:0
           (GETBITS(val, 1, 12) << 17)  | //28:17
           (GETBITS(val, 0, 0)  << 29)  | //29:29
           (insn & GENMASK(31, 30));      //31:30
  }
  case R_SODIUM_LO13S:
  case R_SODIUM_PCREL_LO13S: {
    return (insn & GENMASK(0, 3))       | //3:0
           (GETBITS(val, 1, 5)   << 4)  | //8:4
           (insn & GENMASK(9, 21))      | //21:9
           (GETBITS(val, 6, 12)  << 22) | //28:22
           (GETBITS(val, 0, 0)   << 29) | //29:29
           (insn & GENMASK(31, 30));      //31:30
  }
  }
  return 0; //unreachable
}
