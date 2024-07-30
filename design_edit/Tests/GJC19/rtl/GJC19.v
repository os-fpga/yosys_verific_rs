//***********************************************************
// Functionality: Input is delayed according to controls then 
//                inverted and sent out through output buffer
// Author:        Azfar  
//***********************************************************



module GJC19 (
    input wire clk_i_buf,
    input wire data_i,
    input wire dly_incdec_buf,
    input wire dly_ld_buf,
    input wire dly_adj_buf,
    output wire data_o_inv_delayed_buf,
    output wire [5:0] dly_tap_val_inv_buf
);

    wire data_o;
    wire [5:0] dly_tap_val;
    wire dly_incdec_inv, dly_adj_inv, dly_ld_inv;
    wire enable;
    wire data_i_buf;
    wire clk_buf_i;
    wire clk_i;
    wire dly_incdec;
    wire dly_ld;
    wire dly_adj;
    wire const1;
    wire data_o_inv_delayed;
    wire [5:0] dly_tap_val_inv;

    I_BUF #(.WEAK_KEEPER("PULLDOWN")) buf0_ (clk_i_buf,const1,clk_i);
    I_BUF #(.WEAK_KEEPER("PULLDOWN")) buf1_ (dly_incdec_buf,const1,dly_incdec);
    I_BUF #(.WEAK_KEEPER("PULLDOWN")) buf2_ (dly_ld_buf,const1,dly_ld);
    I_BUF #(.WEAK_KEEPER("PULLDOWN")) buf3_ (dly_adj_buf,const1,dly_adj);
    O_BUF obuf00_ (data_o_inv_delayed,data_o_inv_delayed_buf);
    O_BUF obuf0_ (dly_tap_val_inv[0], dly_tap_val_inv_buf[0]);
    O_BUF obuf1_ (dly_tap_val_inv[1], dly_tap_val_inv_buf[1]);
    O_BUF obuf2_ (dly_tap_val_inv[2], dly_tap_val_inv_buf[2]);
    O_BUF obuf3_ (dly_tap_val_inv[3], dly_tap_val_inv_buf[3]);
    O_BUF obuf4_ (dly_tap_val_inv[4], dly_tap_val_inv_buf[4]);
    O_BUF obuf5_ (dly_tap_val_inv[5], dly_tap_val_inv_buf[5]);

    assign const1 = 1;

    CLK_BUF clock_buffer (clk_i,clk_buf_i);

    assign data_o_inv_delayed   = ~data_o;
    assign enable               = 1;
    assign dly_incdec_inv       = ~dly_incdec;
    assign dly_adj_inv          = ~dly_adj;
    assign dly_ld_inv           = ~dly_ld;
    assign dly_tap_val_inv      = ~dly_tap_val;

    I_BUF data_buf (data_i,enable,data_i_buf);
    I_DELAY data_i_delay (  .I(data_i_buf),
                            .DLY_LOAD(dly_ld_inv),
                            .DLY_ADJ(dly_adj_inv),
                            .DLY_INCDEC(dly_incdec_inv),
                            .DLY_TAP_VALUE(dly_tap_val),
                            .CLK_IN(clk_buf_i),
                            .O(data_o));
endmodule