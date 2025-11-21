// Simple regression to ensure hw.wire is parsed and handled

hw.module @test_hw_wire(
    in %a : i8,
    in %b : i8,
    out out_passthrough : i8,
    out out_sum : i8
) {
  // Basic wire with a human readable name
  %wire_a = hw.wire %a name "wire_a" : i8

  // Nested wire to ensure chaining through multiple wires works
  %wire_b = hw.wire %wire_a : i8

  // Use the wire result downstream
  %sum = comb.add %wire_b, %b : i8

  hw.output %wire_b, %sum : i8, i8
}

