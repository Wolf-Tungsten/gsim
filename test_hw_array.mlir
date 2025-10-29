// Test file to verify HW array operations supported in CIRCT

hw.module @test_hw_arrays(in %a: i8, in %b: i8, in %c: i8, in %d: i8, in %idx: i2, out out1: !hw.array<3xi8>, out out2: i8, out out3: !hw.array<2xi8>, out out4: !hw.array<4xi8>) {
  // Create an array from values
  %array1 = hw.array_create %a, %b, %c : i8

  // Get an element from array
  %element = hw.array_get %array1[%idx] : !hw.array<3xi8>, i2

  // Slice an array (get first 2 elements starting from index 0)
  %c0 = hw.constant 0 : i2
  %slice = hw.array_slice %array1[%c0] : (!hw.array<3xi8>) -> !hw.array<2xi8>

  // Concatenate arrays
  %array2 = hw.array_create %d, %a : i8
  %concat = hw.array_concat %slice, %array2 : !hw.array<2xi8>, !hw.array<2xi8>

  // Output results
  hw.output %array1, %element, %slice, %concat : !hw.array<3xi8>, i8, !hw.array<2xi8>, !hw.array<4xi8>
}

