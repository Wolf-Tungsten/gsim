// Test file to verify comb dialect operations supported in CIRCT

hw.module @test_comb_ops(in %a: i4, in %b: i4, in %c: i1, out out1: i4, out out2: i1, out out3: i1, out out4: i4) {
  // Arithmetic operations
  %add = comb.add %a, %b : i4
  %mul = comb.mul %a, %b : i4
  %sub = comb.sub %a, %b : i4
  %divu = comb.divu %a, %b : i4
  %divs = comb.divs %a, %b : i4
  %modu = comb.modu %a, %b : i4
  %mods = comb.mods %a, %b : i4

  // Shift operations
  %shl = comb.shl %a, %b : i4
  %shru = comb.shru %a, %b : i4
  %shrs = comb.shrs %a, %b : i4

  // Logical operations
  %and = comb.and %a, %b : i4
  %or = comb.or %a, %b : i4
  %xor = comb.xor %a, %b : i4

  // Comparison operations
  %eq = comb.icmp eq %a, %b : i4
  %ne = comb.icmp ne %a, %b : i4
  %slt = comb.icmp slt %a, %b : i4
  %sle = comb.icmp sle %a, %b : i4
  %sgt = comb.icmp sgt %a, %b : i4
  %sge = comb.icmp sge %a, %b : i4
  %ult = comb.icmp ult %a, %b : i4
  %ule = comb.icmp ule %a, %b : i4
  %ugt = comb.icmp ugt %a, %b : i4
  %uge = comb.icmp uge %a, %b : i4

  // Reduction operations
  %parity = comb.parity %a : i4

  // Bit manipulation operations
  %extract = comb.extract %a from 1 : (i4) -> i2
  %concat = comb.concat %a, %b : i4, i4
  %replicate = comb.replicate %a : (i4) -> i8

  // Multiplexer
  %mux = comb.mux %c, %a, %b : i4

  // Output results
  hw.output %add, %eq, %parity, %mux : i4, i1, i1, i4
}
