/* Generated by Yosys 0.18+10 (git sha1 07c42e625, gcc 11.1.0-1ubuntu1~20.04 -fPIC -Os) */

module and2(a, b, clk, reset, c, out);
  input [1:0] clk;
  input a;
  input b;
  output c;
  output out;
  input reset;
  wire out;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:12.23-12.24" *)
  wire [2:0] \$techmap508$abc$493$auto$blifparse.cc:515:parse_blif$494.A ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:13.13-13.14" *)
  wire \$techmap508$abc$493$auto$blifparse.cc:515:parse_blif$494.Y ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:12.23-12.24" *)
  wire [1:0] \$techmap507$abc$493$auto$blifparse.cc:515:parse_blif$496.A ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:13.13-13.14" *)
  wire \$techmap507$abc$493$auto$blifparse.cc:515:parse_blif$496.Y ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:12.23-12.24" *)
  wire [1:0] \$techmap506$abc$493$auto$blifparse.cc:515:parse_blif$495.A ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:13.13-13.14" *)
  wire \$techmap506$abc$493$auto$blifparse.cc:515:parse_blif$495.Y ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:24.16-24.17" *)
  wire \$techmap505$iopadmap$and2.reset.I ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:25.16-25.18" *)
  wire \$techmap505$iopadmap$and2.reset.EN ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:26.16-26.17" *)
  wire \$techmap505$iopadmap$and2.reset.O ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:37.16-37.17" *)
  wire \$techmap504$iopadmap$and2.c.I ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:38.16-38.17" *)
  wire \$techmap504$iopadmap$and2.c.C ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:39.16-39.17" *)
  wire \$techmap504$iopadmap$and2.c.O ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:24.16-24.17" *)
  wire \$techmap503$iopadmap$and2.b.I ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:25.16-25.18" *)
  wire \$techmap503$iopadmap$and2.b.EN ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:26.16-26.17" *)
  wire \$techmap503$iopadmap$and2.b.O ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:24.16-24.17" *)
  wire \$techmap502$iopadmap$and2.a.I ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:25.16-25.18" *)
  wire \$techmap502$iopadmap$and2.a.EN ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:26.16-26.17" *)
  wire \$techmap502$iopadmap$and2.a.O ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:24.16-24.17" *)
  wire \$techmap501$iopadmap$and2.clk.I ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:25.16-25.18" *)
  wire \$techmap501$iopadmap$and2.clk.EN ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:26.16-26.17" *)
  wire \$techmap501$iopadmap$and2.clk.O ;
  wire [1:0] \$iopadmap$clk ;
  (* src = "./Src/and2.v:11.9-11.12" *)
  (* src = "./Src/and2.v:11.9-11.12" *)
  wire [1:0] clk;
  (* src = "./Src/and2.v:9.9-9.10" *)
  (* src = "./Src/and2.v:9.9-9.10" *)
  wire a;
  (* src = "./Src/and2.v:10.9-10.10" *)
  (* src = "./Src/and2.v:10.9-10.10" *)
  wire b;
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:13.14-13.15" *)
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:13.14-13.15" *)
  wire c;
  (* src = "./Src/and2.v:12.9-12.14" *)
  (* src = "./Src/and2.v:12.9-12.14" *)
  wire reset;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:11.12-11.13" *)
  wire \$techmap500$auto$clkbufmap.cc:261:execute$497.O ;
  wire [1:0] \$auto$clkbufmap.cc:294:execute$499 ;
  wire \$iopadmap$a ;
  wire \$abc$221$li0_li0 ;
  wire \$abc$221$li1_li1 ;
  wire \$abc$221$li2_li2 ;
  wire \$iopadmap$b ;
  (* init = 1'h0 *)
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:16.7-16.12" *)
  wire a_reg;
  (* init = 1'h0 *)
  wire \$iopadmap$c ;
  (* init = 1'h0 *)
  (* keep = 32'h00000001 *)
  (* src = "./Src/and2.v:16.14-16.19" *)
  wire b_reg;
  wire \$iopadmap$reset ;
  wire [1:0] \$auto$clkbufmap.cc:262:execute$498 ;
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:10.12-10.13" *)
  wire \$techmap500$auto$clkbufmap.cc:261:execute$497.I ;
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:25.38-25.69" *)
  LUT3 #(
    .INIT_VALUE(8'h40)
  ) \$abc$493$auto$blifparse.cc:515:parse_blif$494  (
    .Y(\$abc$221$li2_li2 ),
    .A({ b_reg, a_reg, \$iopadmap$reset  })
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.b  (
    .O(\$iopadmap$b ),
    .EN(1'h1),
    .I(b)
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.a  (
    .O(\$iopadmap$a ),
    .EN(1'h1),
    .I(a)
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.clk  (
    .O(\$iopadmap$clk[0] ),
    .EN(1'h1),
    .I(clk[0])
  );
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.clk1  (
    .O(\$iopadmap$clk[1] ),
    .EN(1'h1),
    .I(clk[1])
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:41.13-41.44" *)
  O_BUF \$iopadmap$and2.c  (
    .O(c),
    .I(\$iopadmap$c )
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:14.13-14.45" *)
  CLK_BUF \$auto$clkbufmap.cc:261:execute$497  (
    .O(\$auto$clkbufmap.cc:294:execute$499[0] ),
    .I(\$auto$clkbufmap.cc:262:execute$498[0] )
  );
  CLK_BUF \$auto$clkbufmap.cc:261:execute$4971  (
    .O(\$auto$clkbufmap.cc:294:execute$499[1] ),
    .I(\$auto$clkbufmap.cc:262:execute$498[1] )
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/ffs_map.v:10.11-10.70" *)
  DFFRE \$abc$221$auto$blifparse.cc:362:parse_blif$222  (
    .C(\$auto$clkbufmap.cc:294:execute$499[0] ),
    .D(\$abc$221$li0_li0 ),
    .E(1'h1),
    .Q(a_reg),
    .R(1'h1)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/ffs_map.v:10.11-10.70" *)
  DFFRE \$abc$221$auto$blifparse.cc:362:parse_blif$223  (
    .C(\$auto$clkbufmap.cc:294:execute$499[0] ),
    .D(\$abc$221$li1_li1 ),
    .E(1'h1),
    .Q(b_reg),
    .R(1'h1)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/ffs_map.v:10.11-10.70" *)
  DFFRE \$abc$221$auto$blifparse.cc:362:parse_blif$224  (
    .C(\$auto$clkbufmap.cc:294:execute$499[0] ),
    .D(\$abc$221$li2_li2 ),
    .E(1'h1),
    .Q(\$iopadmap$c ),
    .R(1'h1)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:21.38-21.69" *)
  LUT2 #(
    .INIT_VALUE(4'h4)
  ) \$abc$493$auto$blifparse.cc:515:parse_blif$496  (
    .Y(\$abc$221$li0_li0 ),
    .A({ \$iopadmap$a , \$iopadmap$reset  })
  );
  LUT2 #(
    .INIT_VALUE(4'h4)
  ) \$abc$493$auto$blifparse.cc:515:parse_blif$4961  (
    .Y(out ),
    .A({ \$iopadmap$a , \$auto$clkbufmap.cc:294:execute$499[1]  })
  );
  (* keep = 32'h00000001 *)
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/io_cell_final_map.v:29.41-29.81" *)
  I_BUF #(
    .WEAK_KEEPER("NONE")
  ) \$iopadmap$and2.reset  (
    .O(\$iopadmap$reset ),
    .EN(1'h1),
    .I(reset)
  );
  (* module_not_derived = 32'h00000001 *)
  (* src = "/nfs_scratch/scratch/eda/behzad/pp/yosys_verific_rs/yosys/install/bin/../share/yosys/rapidsilicon/genesis3/lut_map.v:21.38-21.69" *)
  LUT2 #(
    .INIT_VALUE(4'h4)
  ) \$abc$493$auto$blifparse.cc:515:parse_blif$495  (
    .Y(\$abc$221$li1_li1 ),
    .A({ \$iopadmap$b , \$iopadmap$reset  })
  );
  assign \$auto$clkbufmap.cc:262:execute$498[0]  = \$iopadmap$clk[0] ;
  assign \$auto$clkbufmap.cc:262:execute$498[1]  = \$iopadmap$clk[1] ;
endmodule