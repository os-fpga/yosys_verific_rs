module primitive_example_design_5(
    input [2:0] in,
    input ibuf2_en,
    input rst,
    input clk, 
    input ibuf1_en,ibuf2_en,ibuf3_en,ibuf4_en,ibuf5_en,
    input obuft_ds_en1,obuft_ds_en2,obuft_ds_en3,
    input [2:0] iddr_en,
    output [2:0] q_p,
    output [2:0] q_n
);

    wire [2:0] iddr_out;
    wire [2:0] dffre_out;
    wire [2:0] i_buf_out;
    wire rst_i_buf_out;

    I_BUF clk_buf_inst (.I(clk),.EN(ibuf1_en),.O(clk_buf_out));

    I_DDR iddr_ist1 (.D(i_buf_out[0]),.R(rst),.E(iddr_en[0]),.C(clk_buf_out),.Q(iddr_out[0]));
    I_DDR iddr_ist2 (.D(i_buf_out[1]),.R(rst),.E(iddr_en[1]),.C(clk_buf_out),.Q(iddr_out[1]));
    I_DDR iddr_ist3 (.D(i_buf_out[2]),.R(rst),.E(iddr_en[2]),.C(clk_buf_out),.Q(iddr_out[2]));

    O_BUFT_DS o_buft_inst1 (.I(dffre_out[0]),.T(obuft_ds_en1),.O_P(q_p[0]),.O_N(q_p[0]));
    O_BUFT_DS o_buft_inst2 (.I(dffre_out[1]),.T(obuft_ds_en2),.O_P(q_p[1]),.O_N(q_p[1]));
    O_BUFT_DS o_buft_inst3 (.I(dffre_out[2]),.T(obuft_ds_en3),.O_P(q_p[2]),.O_N(q_p[2]));
    
    I_BUF ibuf_inst1 (.I(in[0]),.EN(ibuf2_en),.O(i_buf_out[0]));
    I_BUF ibuf_inst2 (.I(in[1]),.EN(ibuf3_en),.O(i_buf_out[1]));
    I_BUF ibuf_inst3 (.I(in[2]),.EN(ibuf4_en),.O(i_buf_out[2]));
    I_BUF ibuf_inst4 (.I(rst),.EN(ibuf5_en),.O(rst_i_buf_out));

    DFFRE ff_inst1 (.D(iddr_out[0]),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out[0]));
    DFFRE ff_inst2 (.D(iddr_out[1]),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out[1]));
    DFFRE ff_inst3 (.D(iddr_out[2]),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out[2]));

endmodule
