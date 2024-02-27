module primitive_example_design_6(clk,in,rst,q_p,q_n,oddr_en,ibuf_oe1,ibuf_oe2,ibuf_oe3,ibuf_oe4);

input [1:0] in;
input clk, rst;
input ibuf_oe1,ibuf_oe2,ibuf_oe3,ibuf_oe4;
input oddr_en;
output q_p;
output q_n;

wire [1:0] oddr_out;
wire [1:0] dffre_out;
wire [1:0] i_buf_out;
wire rst_i_buf_out;

I_BUF clk_buf_inst (.I(clk),.EN(ibuf_oe1),.O(clk_buf_out));

O_DDR iddr_ist1 (.D(i_buf_out),.R(rst),.E(oddr_en),.C(clk_buf_out),.Q(oddr_out));

O_BUFT_DS o_buft_inst1 (.T(dffre_out),.I(in[0]),.O_N(q_n),.O_P(q_p));

I_BUF ibuf_inst1 (.I(in[0]),.EN(ibuf_oe2),.O(i_buf_out[0]));
I_BUF ibuf_inst2 (.I(in[1]),.EN(ibuf_oe3),.O(i_buf_out[1]));
I_BUF ibuf_inst4 (.I(rst),.EN(ibuf_oe4),.O(rst_i_buf_out));

DFFRE ff_inst1 (.D(oddr_out),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffre_out));

endmodule
