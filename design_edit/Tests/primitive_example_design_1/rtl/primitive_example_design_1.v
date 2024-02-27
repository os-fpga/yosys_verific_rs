module primitive_example_design_1(clk,in,rst,Q,mux1_sel,mux2_sel,P,G,ram_addr,ram_we,buft_out,obuft_oe,ibuf0_en,ibuf1_en,ibuf2_en,ibuf3_en,ibuf4_en,ibuf5_en,ibuf6_en,ibuf7_en,ibuf8_en,ibuf9_en,ibuf10_en,ibuf11_en,ibuf12_en,ibuf13_en,ibuf14_en,ibuf15_en,ibuf16_en);
    input [2:0] in;
    input clk, rst;
    input mux1_sel,mux2_sel;
    input ibuf0_en,ibuf1_en,ibuf2_en,ibuf3_en,ibuf4_en,ibuf5_en,ibuf6_en,ibuf7_en,ibuf8_en,ibuf9_en,ibuf10_en,ibuf11_en,ibuf12_en,ibuf13_en,ibuf14_en,ibuf15_en,ibuf16_en;
    input P,G;
    input [5:0] ram_addr;
    input ram_we;
    input obuft_oe;
    output Q,buft_out;

    wire [2:0] i_buf_out;
    wire [5:0] i_buf_ram_addr;
    wire in_buf_out,clk_buf_out;
    wire lut_out;
    wire rst_i_buf_out,i_buf_mux1_sel,i_buf_mux2_sel;
    wire out,p_ibuf,g_ibuf; 
    wire dffnre_out;
    wire ac_out, acc_out;
    wire mux2_out;
    wire Q_buff_in;
    wire ram_out,i_buf_ram_we;
    wire inf_q,ibuf_obuft_oe;

    I_BUF clk_buf_inst (.I(clk),.EN(ibuf0_en),.O(clk_buf_out));
    
    I_BUF ibuf_inst1 (.I(in[0]),.EN(ibuf1_en),.O(i_buf_out[0]));
    I_BUF ibuf_inst2 (.I(in[1]),.EN(ibuf2_en),.O(i_buf_out[1]));
    I_BUF ibuf_inst3 (.I(in[2]),.EN(ibuf3_en),.O(i_buf_out[2]));
    I_BUF ibuf_inst4 (.I(rst),.EN(ibuf4_en),.O(rst_i_buf_out));
    I_BUF ibuf_inst5 (.I(mux1_sel),.EN(ibuf5_en),.O(i_buf_mux1_sel));
    I_BUF ibuf_inst6 (.I(mux2_sel),.EN(ibuf6_en),.O(i_buf_mux2_sel));
    I_BUF ibuf_inst7 (.I(P),.EN(ibuf7_en),.O(p_ibuf));
    I_BUF ibuf_inst8 (.I(G),.EN(ibuf8_en),.O(g_ibuf));
    I_BUF ibuf_inst9 (.I(ram_we),.EN(ibuf9_en),.O(i_buf_ram_we));
    I_BUF ibuf_inst10 (.I(ram_addr[0]),.EN(ibuf10_en),.O(i_buf_ram_addr[0]));
    I_BUF ibuf_inst11 (.I(ram_addr[1]),.EN(ibuf11_en),.O(i_buf_ram_addr[1]));
    I_BUF ibuf_inst12 (.I(ram_addr[2]),.EN(ibuf12_en),.O(i_buf_ram_addr[2]));
    I_BUF ibuf_inst13 (.I(ram_addr[3]),.EN(ibuf13_en),.O(i_buf_ram_addr[3]));
    I_BUF ibuf_inst14 (.I(ram_addr[4]),.EN(ibuf14_en),.O(i_buf_ram_addr[4]));
    I_BUF ibuf_inst15 (.I(ram_addr[5]),.EN(ibuf15_en),.O(i_buf_ram_addr[5]));
    I_BUF ibuf_inst16 (.I(obuft_oe),.EN(ibuf16_en),.O(ibuf_obuft_oe));

    DFFNRE ffn_inst (.D(lut_out),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(dffnre_out));

    assign out = i_buf_mux1_sel ? dffnre_out : !dffnre_out;

    flip_flop ff_inst1 (.clk(clk),.rst(rst),.D(ac_out),.Q(inf_q));

    O_BUFT obuft_inst (.I(inf_q),.T(ibuf_obuft_oe),.O(buft_out));

    O_BUF o_buff_inst (.I(ram_out),.O(Q));

    DFFRE ff_inst (.D(mux2_out),.R(rst_i_buf_out),.E(1'b1),.C(clk_buf_out),.Q(Q_buff_in));

    assign mux2_out = i_buf_mux2_sel ? ac_out : acc_out;
    
    CARRY_CHAIN carry_chain_inst (.P(p_ibuf),.G(g_ibuf),.CIN(out),.O(ac_out),.COUT(acc_out));

    infer_single_port_ram ram_inst (.data(Q_buff_in),.addr(i_buf_ram_addr),.we(i_buf_ram_we),.clk(clk),.q(ram_out));

    always @(*) begin
        case(i_buf_out)
            3'b000 : lut_out = 0;
            3'b001 : lut_out = 1;
            3'b010 : lut_out = 1;
            3'b011 : lut_out = 0;
            3'b100 : lut_out = 1;
            3'b101 : lut_out = 1;
            3'b110 : lut_out = 1;
            3'b111 : lut_out = 0;
            default: lut_out = 1;
        endcase
    end
endmodule

module flip_flop(
    input rst,clk,
    input D,
    output reg Q
);
    always @ (posedge clk) begin
        if (rst)
            Q <= 0;
        else
            Q <= D;
    end
endmodule

module infer_single_port_ram 
#(parameter DATA_WIDTH=8, parameter ADDR_WIDTH=6)
(
	input [(DATA_WIDTH-1):0] data,
	input [(ADDR_WIDTH-1):0] addr,
	input we, clk,
	output [(DATA_WIDTH-1):0] q
);

	reg [DATA_WIDTH-1:0] ram[2**ADDR_WIDTH-1:0];

	reg [ADDR_WIDTH-1:0] addr_reg;

	always @ (posedge clk)
	begin
		if (we)
			ram[addr] <= data;

		addr_reg <= addr;
	end

	assign q = ram[addr_reg];

endmodule
