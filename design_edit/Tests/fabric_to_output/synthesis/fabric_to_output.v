/* Generated by Yosys 0.38 (git sha1 d2189b06a, gcc 11.2.1 -fPIC -Os) */

module fabric_to_output(a, b, clk, reset, c);
  input a;
  input b;
  output c;
  input clk;
  input reset;
  wire \$abc$221$li0_li0 ;
  wire \$abc$221$li1_li1 ;
  wire \$abc$221$li2_li2 ;
  wire \$auto$clkbufmap.cc:339:execute$499 ;
  wire \$iopadmap$a ;
  wire \$iopadmap$b ;
  (* init = 1'h0 *)
  wire \$iopadmap$c ;
  wire \$iopadmap$clk ;
  wire \$iopadmap$reset ;
  (* src = "./Src/and2.v:9.9-9.10" *)
  (* src = "./Src/and2.v:9.9-9.10" *)
  wire a;
  (* init = 1'h0 *)
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:16.7-16.12" *)
  wire a_reg;
  (* src = "./Src/and2.v:10.9-10.10" *)
  (* src = "./Src/and2.v:10.9-10.10" *)
  wire b;
  (* init = 1'h0 *)
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:16.14-16.19" *)
  wire b_reg;
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:13.14-13.15" *)
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:13.14-13.15" *)
  wire c;
  (* src = "./Src/and2.v:11.9-11.12" *)
  (* src = "./Src/and2.v:11.9-11.12" *)
  wire clk;
  (* src = "./Src/and2.v:12.9-12.14" *)
  (* src = "./Src/and2.v:12.9-12.14" *)
  wire reset;
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/ffs_map.v:10.11-10.70" *)
  DFFRE \$abc$221$auto$blifparse.cc:377:parse_blif$222  (
    .C(\$auto$clkbufmap.cc:339:execute$499 ),
    .D(\$abc$221$li0_li0 ),
    .E(1'h1),
    .Q(a_reg),
    .R(1'h1)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/ffs_map.v:10.11-10.70" *)
  DFFRE \$abc$221$auto$blifparse.cc:377:parse_blif$223  (
    .C(\$auto$clkbufmap.cc:339:execute$499 ),
    .D(\$abc$221$li1_li1 ),
    .E(1'h1),
    .Q(b_reg),
    .R(1'h1)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/ffs_map.v:10.11-10.70" *)
  DFFRE \$abc$221$auto$blifparse.cc:377:parse_blif$224  (
    .C(\$auto$clkbufmap.cc:339:execute$499 ),
    .D(\$abc$221$li2_li2 ),
    .E(1'h1),
    .Q(c),
    .R(1'h1)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:25.38-25.69" *)
  LUT3 #(
    .INIT_VALUE(8'h40)
  ) \$abc$493$auto$blifparse.cc:535:parse_blif$494  (
    .A({ b_reg, a_reg, \$iopadmap$reset  }),
    .Y(\$abc$221$li2_li2 )
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:21.38-21.69" *)
  LUT2 #(
    .INIT_VALUE(4'h4)
  ) \$abc$493$auto$blifparse.cc:535:parse_blif$495  (
    .A({ \$iopadmap$b , \$iopadmap$reset  }),
    .Y(\$abc$221$li1_li1 )
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:21.38-21.69" *)
  LUT2 #(
    .INIT_VALUE(4'h4)
  ) \$abc$493$auto$blifparse.cc:535:parse_blif$496  (
    .A({ \$iopadmap$a , \$iopadmap$reset  }),
    .Y(\$abc$221$li0_li0 )
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:14.13-14.45" *)
  CLK_BUF \$auto$clkbufmap.cc:306:execute$497  (
    .I(\$iopadmap$clk ),
    .O(\$auto$clkbufmap.cc:339:execute$499 )
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.a  (
    .EN(1'h1),
    .I(a),
    .O(\$iopadmap$a )
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.b  (
    .EN(1'h1),
    .I(b),
    .O(\$iopadmap$b )
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.clk  (
    .EN(1'h1),
    .I(clk),
    .O(\$iopadmap$clk )
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/jul18/Raptor/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.reset  (
    .EN(1'h1),
    .I(reset),
    .O(\$iopadmap$reset )
  );
endmodule