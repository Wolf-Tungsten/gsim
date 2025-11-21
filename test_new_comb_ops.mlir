hw.module @test_new_comb_ops(in %input: i8, in %a: i1, in %b: i1, out out: i16) {
    // 测试各种 comb 操作 除了 truthtable
    %add_result = comb.add %input, %input : i8
    %sub_result = comb.sub %add_result, %input : i8
    %mul_result = comb.mul %input, %input : i8
    %and_result = comb.and %input, %input : i8
    %or_result = comb.or %input, %input : i8
    %xor_result = comb.xor %input, %input : i8

    // 测试比较操作
    %eq_result = comb.icmp eq %input, %input : i8
    %ne_result = comb.icmp ne %input, %input : i8
    %lt_result = comb.icmp slt %input, %input : i8

    // 测试位操作
    %shl_result = comb.shl %input, %input : i8
    %shr_result = comb.shru %input, %input : i8

    // 测试拼接和提取
    %concat_result = comb.concat %input, %input : i8, i8
    %extract_result = comb.extract %concat_result from 0 : (i16) -> i8

    // 测试多路选择器
    %mux_result = comb.mux %a, %add_result, %sub_result : i8

    // 测试奇偶校验
    %parity_result = comb.parity %input : i8

    // 测试位复制
    %replicate_result = comb.replicate %input : (i8) -> i16

    // 输出最终结果
    hw.output %replicate_result : i16
}
