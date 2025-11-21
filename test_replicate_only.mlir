hw.module @test_replicate_only(in %input: i8, out out: i16) {
    // 测试位复制操作
    %replicate_result = comb.replicate %input : (i8) -> i16
    hw.output %replicate_result : i16
}
