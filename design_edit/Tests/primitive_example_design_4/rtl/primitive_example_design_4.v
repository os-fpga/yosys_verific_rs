module primitive_example_design_4(
    input [2:0] in,
    input clk, rst,
    input ibuf1_en,ibuf2_en,ibuf3_en,ibuf4_en,ibuf5_en,
    output [2:0] q_n,
    output [2:0] q_p,
    );

    wire [2:0] i_buf_out;
    wire rst_i_buf_out;
    wire [2:0] dffre_out;

    CLK_BUF clk_buf_inst (.I(clk),.O(clk_buf_out));

    O_BUF_DS obuf_ds_inst1 (.I(dffre_out[0]),.O_P(q_p[0]),.O_N(q_n[0]));
    O_BUF_DS obuf_ds_inst2 (.I(dffre_out[1]),.O_P(q_p[1]),.O_N(q_n[1]));
    O_BUF_DS obuf_ds_inst3 (.I(dffre_out[2]),.O_P(q_p[2]),.O_N(q_n[2]));
    
    I_BUF ibuf_inst1 (.I(in[0]),.EN(ibuf2_en),.O(i_buf_out[0]));
    I_BUF ibuf_inst2 (.I(in[1]),.EN(ibuf3_en),.O(i_buf_out[1]));
    I_BUF ibuf_inst3 (.I(in[2]),.EN(ibuf4_en),.O(i_buf_out[2]));
    I_BUF ibuf_inst4 (.I(rst),.EN(ibuf5_en),.O(rst_i_buf_out));

    DFFRE ff_inst1 (.D(i_buf_out[0]),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out[0]));
    DFFRE ff_inst2 (.D(i_buf_out[1]),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out[1]));
    DFFRE ff_inst3 (.D(i_buf_out[2]),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out[2]));

endmodule
