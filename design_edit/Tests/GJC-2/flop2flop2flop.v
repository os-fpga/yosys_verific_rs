/////////////////////////////////////////
//  Functionality: flop to flop path
//  Author:        George Chen
////////////////////////////////////////
// `timescale 1ns / 1ps


module flop2flop2flop(
  din,
  dout,
  clk);

input din;
input clk;
output reg dout;

reg q1 ;
   reg q2 ;
   

always @(posedge clk)
    begin
      q1 <= din ;
	end

always @(posedge clk)
    begin
      q2 <= q1 ;
	end

always @(posedge clk)
    begin 
	    dout <= q2 ;
	end
		
endmodule
