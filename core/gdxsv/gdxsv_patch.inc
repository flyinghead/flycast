//
// section gdx.data
//
symbols_["gdx_rpc"] = 0x0c4e0200;
symbols_["dummy"] = 0x0c4e0218;
gdxsv_WriteMem16(0x0c4e0218u, 0x0000u); // .word 0x0000 
symbols_["host_addr_0"] = 0x0c4e021c;
gdxsv_WriteMem16(0x0c4e021cu, 0x0000u); // .word 0x0000 
symbols_["host_addr_list"] = 0x0c4e0220;
gdxsv_WriteMem16(0x0c4e0220u, 0x0000u); // .word 0x0000 
symbols_["host_entry"] = 0x0c4e0224;
symbols_["print_buf"] = 0x0c4e0238;
symbols_["ppp_status_ok"] = 0x0c4e0638;
gdxsv_WriteMem16(0x0c4e0638u, 0xa701u); // bra	c4e143e <patch_id+0xde6> 
gdxsv_WriteMem16(0x0c4e063au, 0xc0a8u); // mov.b	r0,@(168,gbr) 
gdxsv_WriteMem16(0x0c4e063cu, 0xa702u); // bra	c4e1444 <patch_id+0xdec> 
gdxsv_WriteMem16(0x0c4e063eu, 0xc0a8u); // mov.b	r0,@(168,gbr) 
gdxsv_WriteMem16(0x0c4e0640u, 0x0004u); // mov.b	r0,@(r0,r0) 
gdxsv_WriteMem16(0x0c4e0642u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x0c4e0644u, 0x004bu); // .word 0x004b 
gdxsv_WriteMem16(0x0c4e0646u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x0c4e0648u, 0x0001u); // .word 0x0001 
symbols_["print_buf_pos"] = 0x0c4e064c;
gdxsv_WriteMem16(0x0c4e064cu, 0x0000u); // .word 0x0000 
symbols_["is_online"] = 0x0c4e0650;
gdxsv_WriteMem16(0x0c4e0650u, 0x0000u); // .word 0x0000 
symbols_["disk"] = 0x0c4e0654;
gdxsv_WriteMem16(0x0c4e0654u, 0x0000u); // .word 0x0000 
symbols_["patch_id"] = 0x0c4e0658;
gdxsv_WriteMem16(0x0c4e0658u, 0x0000u); // .word 0x0000 
//
// section gdx.func
//
symbols_["gdx_sock_create"] = 0x0c4f0000;
gdxsv_WriteMem16(0x8c4f0000u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0002u, 0xe001u); // mov	#1,r0 
symbols_["gdx_sock_close"] = 0x0c4f0004;
gdxsv_WriteMem16(0x8c4f0004u, 0xd109u); // mov.l	c4f002c <gdx_sock_close+0x28>,r1	! c4e0650 <is_online> 
gdxsv_WriteMem16(0x8c4f0006u, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f0008u, 0xe302u); // mov	#2,r3 
gdxsv_WriteMem16(0x8c4f000au, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f000cu, 0xe000u); // mov	#0,r0 
gdxsv_WriteMem16(0x8c4f000eu, 0xd108u); // mov.l	c4f0030 <gdx_sock_close+0x2c>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f0010u, 0x2132u); // mov.l	r3,@r1 
gdxsv_WriteMem16(0x8c4f0012u, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f0014u, 0x1142u); // mov.l	r4,@(8,r1) 
gdxsv_WriteMem16(0x8c4f0016u, 0x1123u); // mov.l	r2,@(12,r1) 
gdxsv_WriteMem16(0x8c4f0018u, 0x1124u); // mov.l	r2,@(16,r1) 
gdxsv_WriteMem16(0x8c4f001au, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f001cu, 0xd205u); // mov.l	c4f0034 <gdx_sock_close+0x30>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f001eu, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f0020u, 0xd205u); // mov.l	c4f0038 <gdx_sock_close+0x34>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f0022u, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f0024u, 0x5111u); // mov.l	@(4,r1),r1 
gdxsv_WriteMem16(0x8c4f0026u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0028u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f002au, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f002cu, 0x0650u); // .word 0x0650 
gdxsv_WriteMem16(0x8c4f002eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0030u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f0032u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0034u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f0036u, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f0038u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f003au, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_gethostbyname"] = 0x0c4f003c;
gdxsv_WriteMem16(0x8c4f003cu, 0xd207u); // mov.l	c4f005c <gdx_gethostbyname+0x20>,r2	! c4e021c <host_addr_0> 
gdxsv_WriteMem16(0x8c4f003eu, 0x910cu); // mov.w	c4f005a <gdx_gethostbyname+0x1e>,r1	! 707 
gdxsv_WriteMem16(0x8c4f0040u, 0xd007u); // mov.l	c4f0060 <gdx_gethostbyname+0x24>,r0	! c4e0224 <host_entry> 
gdxsv_WriteMem16(0x8c4f0042u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0044u, 0xe100u); // mov	#0,r1 
gdxsv_WriteMem16(0x8c4f0046u, 0x2012u); // mov.l	r1,@r0 
gdxsv_WriteMem16(0x8c4f0048u, 0x1011u); // mov.l	r1,@(4,r0) 
gdxsv_WriteMem16(0x8c4f004au, 0xe102u); // mov	#2,r1 
gdxsv_WriteMem16(0x8c4f004cu, 0x1012u); // mov.l	r1,@(8,r0) 
gdxsv_WriteMem16(0x8c4f004eu, 0xe101u); // mov	#1,r1 
gdxsv_WriteMem16(0x8c4f0050u, 0x1013u); // mov.l	r1,@(12,r0) 
gdxsv_WriteMem16(0x8c4f0052u, 0xd104u); // mov.l	c4f0064 <gdx_gethostbyname+0x28>,r1	! c4e0220 <host_addr_list> 
gdxsv_WriteMem16(0x8c4f0054u, 0x1014u); // mov.l	r1,@(16,r0) 
gdxsv_WriteMem16(0x8c4f0056u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0058u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f005au, 0x0707u); // mul.l	r0,r7 
gdxsv_WriteMem16(0x8c4f005cu, 0x021cu); // mov.b	@(r0,r1),r2 
gdxsv_WriteMem16(0x8c4f005eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0060u, 0x0224u); // mov.b	r2,@(r0,r2) 
gdxsv_WriteMem16(0x8c4f0062u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0064u, 0x0220u); // .word 0x0220 
gdxsv_WriteMem16(0x8c4f0066u, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_connect_sock"] = 0x0c4f0068;
gdxsv_WriteMem16(0x8c4f0068u, 0xd10eu); // mov.l	c4f00a4 <gdx_connect_sock+0x3c>,r1	! c4e0650 <is_online> 
gdxsv_WriteMem16(0x8c4f006au, 0xe301u); // mov	#1,r3 
gdxsv_WriteMem16(0x8c4f006cu, 0x8551u); // mov.w	@(2,r5),r0 
gdxsv_WriteMem16(0x8c4f006eu, 0x5751u); // mov.l	@(4,r5),r7 
gdxsv_WriteMem16(0x8c4f0070u, 0x2132u); // mov.l	r3,@r1 
gdxsv_WriteMem16(0x8c4f0072u, 0x650du); // extu.w	r0,r5 
gdxsv_WriteMem16(0x8c4f0074u, 0x9114u); // mov.w	c4f00a0 <gdx_connect_sock+0x38>,r1	! 707 
gdxsv_WriteMem16(0x8c4f0076u, 0x6558u); // swap.b	r5,r5 
gdxsv_WriteMem16(0x8c4f0078u, 0x3710u); // cmp/eq	r1,r7 
gdxsv_WriteMem16(0x8c4f007au, 0xd10bu); // mov.l	c4f00a8 <gdx_connect_sock+0x40>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f007cu, 0x0229u); // movt	r2 
gdxsv_WriteMem16(0x8c4f007eu, 0x2132u); // mov.l	r3,@r1 
gdxsv_WriteMem16(0x8c4f0080u, 0xe300u); // mov	#0,r3 
gdxsv_WriteMem16(0x8c4f0082u, 0x1131u); // mov.l	r3,@(4,r1) 
gdxsv_WriteMem16(0x8c4f0084u, 0x6023u); // mov	r2,r0 
gdxsv_WriteMem16(0x8c4f0086u, 0x1122u); // mov.l	r2,@(8,r1) 
gdxsv_WriteMem16(0x8c4f0088u, 0xca01u); // xor	#1,r0 
gdxsv_WriteMem16(0x8c4f008au, 0x1173u); // mov.l	r7,@(12,r1) 
gdxsv_WriteMem16(0x8c4f008cu, 0x7001u); // add	#1,r0 
gdxsv_WriteMem16(0x8c4f008eu, 0x1154u); // mov.l	r5,@(16,r1) 
gdxsv_WriteMem16(0x8c4f0090u, 0x1135u); // mov.l	r3,@(20,r1) 
gdxsv_WriteMem16(0x8c4f0092u, 0xd306u); // mov.l	c4f00ac <gdx_connect_sock+0x44>,r3	! 400000 
gdxsv_WriteMem16(0x8c4f0094u, 0x6730u); // mov.b	@r3,r7 
gdxsv_WriteMem16(0x8c4f0096u, 0xd306u); // mov.l	c4f00b0 <gdx_connect_sock+0x48>,r3	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f0098u, 0x2370u); // mov.b	r7,@r3 
gdxsv_WriteMem16(0x8c4f009au, 0x5111u); // mov.l	@(4,r1),r1 
gdxsv_WriteMem16(0x8c4f009cu, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f009eu, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f00a0u, 0x0707u); // mul.l	r0,r7 
gdxsv_WriteMem16(0x8c4f00a2u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f00a4u, 0x0650u); // .word 0x0650 
gdxsv_WriteMem16(0x8c4f00a6u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f00a8u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f00aau, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f00acu, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f00aeu, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f00b0u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f00b2u, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_select"] = 0x0c4f00b4;
gdxsv_WriteMem16(0x8c4f00b4u, 0xd10bu); // mov.l	c4f00e4 <gdx_select+0x30>,r1	! c4e0650 <is_online> 
gdxsv_WriteMem16(0x8c4f00b6u, 0x6112u); // mov.l	@r1,r1 
gdxsv_WriteMem16(0x8c4f00b8u, 0x2118u); // tst	r1,r1 
gdxsv_WriteMem16(0x8c4f00bau, 0x8d10u); // bt.s	c4f00de <gdx_select+0x2a> 
gdxsv_WriteMem16(0x8c4f00bcu, 0xe0ffu); // mov	#-1,r0 
gdxsv_WriteMem16(0x8c4f00beu, 0xd10au); // mov.l	c4f00e8 <gdx_select+0x34>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f00c0u, 0xe205u); // mov	#5,r2 
gdxsv_WriteMem16(0x8c4f00c2u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f00c4u, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f00c6u, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f00c8u, 0x1122u); // mov.l	r2,@(8,r1) 
gdxsv_WriteMem16(0x8c4f00cau, 0x1123u); // mov.l	r2,@(12,r1) 
gdxsv_WriteMem16(0x8c4f00ccu, 0x1124u); // mov.l	r2,@(16,r1) 
gdxsv_WriteMem16(0x8c4f00ceu, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f00d0u, 0xd206u); // mov.l	c4f00ec <gdx_select+0x38>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f00d2u, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f00d4u, 0xd206u); // mov.l	c4f00f0 <gdx_select+0x3c>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f00d6u, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f00d8u, 0x5111u); // mov.l	@(4,r1),r1 
gdxsv_WriteMem16(0x8c4f00dau, 0x2118u); // tst	r1,r1 
gdxsv_WriteMem16(0x8c4f00dcu, 0x600au); // negc	r0,r0 
gdxsv_WriteMem16(0x8c4f00deu, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f00e0u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f00e2u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f00e4u, 0x0650u); // .word 0x0650 
gdxsv_WriteMem16(0x8c4f00e6u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f00e8u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f00eau, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f00ecu, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f00eeu, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f00f0u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f00f2u, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_lbs_sock_write"] = 0x0c4f00f4;
gdxsv_WriteMem16(0x8c4f00f4u, 0xd107u); // mov.l	c4f0114 <gdx_lbs_sock_write+0x20>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f00f6u, 0xe204u); // mov	#4,r2 
gdxsv_WriteMem16(0x8c4f00f8u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f00fau, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f00fcu, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f00feu, 0x1152u); // mov.l	r5,@(8,r1) 
gdxsv_WriteMem16(0x8c4f0100u, 0x1163u); // mov.l	r6,@(12,r1) 
gdxsv_WriteMem16(0x8c4f0102u, 0x1124u); // mov.l	r2,@(16,r1) 
gdxsv_WriteMem16(0x8c4f0104u, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f0106u, 0xd204u); // mov.l	c4f0118 <gdx_lbs_sock_write+0x24>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f0108u, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f010au, 0xd204u); // mov.l	c4f011c <gdx_lbs_sock_write+0x28>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f010cu, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f010eu, 0x5011u); // mov.l	@(4,r1),r0 
gdxsv_WriteMem16(0x8c4f0110u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0112u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0114u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f0116u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0118u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f011au, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f011cu, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f011eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_lbs_sock_read"] = 0x0c4f0120;
gdxsv_WriteMem16(0x8c4f0120u, 0xd10au); // mov.l	c4f014c <gdx_lbs_sock_read+0x2c>,r1	! c4e0650 <is_online> 
gdxsv_WriteMem16(0x8c4f0122u, 0x6112u); // mov.l	@r1,r1 
gdxsv_WriteMem16(0x8c4f0124u, 0x2118u); // tst	r1,r1 
gdxsv_WriteMem16(0x8c4f0126u, 0x8d0fu); // bt.s	c4f0148 <gdx_lbs_sock_read+0x28> 
gdxsv_WriteMem16(0x8c4f0128u, 0xe203u); // mov	#3,r2 
gdxsv_WriteMem16(0x8c4f012au, 0xd109u); // mov.l	c4f0150 <gdx_lbs_sock_read+0x30>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f012cu, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f012eu, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f0130u, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f0132u, 0x1152u); // mov.l	r5,@(8,r1) 
gdxsv_WriteMem16(0x8c4f0134u, 0x1163u); // mov.l	r6,@(12,r1) 
gdxsv_WriteMem16(0x8c4f0136u, 0x1124u); // mov.l	r2,@(16,r1) 
gdxsv_WriteMem16(0x8c4f0138u, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f013au, 0xd206u); // mov.l	c4f0154 <gdx_lbs_sock_read+0x34>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f013cu, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f013eu, 0xd206u); // mov.l	c4f0158 <gdx_lbs_sock_read+0x38>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f0140u, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f0142u, 0x5011u); // mov.l	@(4,r1),r0 
gdxsv_WriteMem16(0x8c4f0144u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0146u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0148u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f014au, 0xe0ffu); // mov	#-1,r0 
gdxsv_WriteMem16(0x8c4f014cu, 0x0650u); // .word 0x0650 
gdxsv_WriteMem16(0x8c4f014eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0150u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f0152u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0154u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f0156u, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f0158u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f015au, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_mcs_sock_read"] = 0x0c4f015c;
gdxsv_WriteMem16(0x8c4f015cu, 0xd001u); // mov.l	c4f0164 <gdx_mcs_sock_read+0x8>,r0	! c4f0120 <gdx_lbs_sock_read> 
gdxsv_WriteMem16(0x8c4f015eu, 0x402bu); // jmp	@r0 
gdxsv_WriteMem16(0x8c4f0160u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0162u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0164u, 0x0120u); // .word 0x0120 
gdxsv_WriteMem16(0x8c4f0166u, 0x0c4fu); // mac.l	@r4+,@r12+ 
symbols_["gdx_mcs_sock_write"] = 0x0c4f0168;
gdxsv_WriteMem16(0x8c4f0168u, 0xd107u); // mov.l	c4f0188 <gdx_mcs_sock_write+0x20>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f016au, 0xe204u); // mov	#4,r2 
gdxsv_WriteMem16(0x8c4f016cu, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f016eu, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f0170u, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f0172u, 0x1152u); // mov.l	r5,@(8,r1) 
gdxsv_WriteMem16(0x8c4f0174u, 0x1163u); // mov.l	r6,@(12,r1) 
gdxsv_WriteMem16(0x8c4f0176u, 0x1124u); // mov.l	r2,@(16,r1) 
gdxsv_WriteMem16(0x8c4f0178u, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f017au, 0xd204u); // mov.l	c4f018c <gdx_mcs_sock_write+0x24>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f017cu, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f017eu, 0xd204u); // mov.l	c4f0190 <gdx_mcs_sock_write+0x28>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f0180u, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f0182u, 0x5011u); // mov.l	@(4,r1),r0 
gdxsv_WriteMem16(0x8c4f0184u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0186u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0188u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f018au, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f018cu, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f018eu, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f0190u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f0192u, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_softreset_disconnect"] = 0x0c4f0194;
gdxsv_WriteMem16(0x8c4f0194u, 0x2f86u); // mov.l	r8,@-r15 
gdxsv_WriteMem16(0x8c4f0196u, 0xd819u); // mov.l	c4f01fc <gdx_softreset_disconnect+0x68>,r8	! c4e0654 <disk> 
gdxsv_WriteMem16(0x8c4f0198u, 0x4f22u); // sts.l	pr,@-r15 
gdxsv_WriteMem16(0x8c4f019au, 0x6082u); // mov.l	@r8,r0 
gdxsv_WriteMem16(0x8c4f019cu, 0x8801u); // cmp/eq	#1,r0 
gdxsv_WriteMem16(0x8c4f019eu, 0x8d06u); // bt.s	c4f01ae <gdx_softreset_disconnect+0x1a> 
gdxsv_WriteMem16(0x8c4f01a0u, 0x8802u); // cmp/eq	#2,r0 
gdxsv_WriteMem16(0x8c4f01a2u, 0x8d0cu); // bt.s	c4f01be <gdx_softreset_disconnect+0x2a> 
gdxsv_WriteMem16(0x8c4f01a4u, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f01a6u, 0x6023u); // mov	r2,r0 
gdxsv_WriteMem16(0x8c4f01a8u, 0x4f26u); // lds.l	@r15+,pr 
gdxsv_WriteMem16(0x8c4f01aau, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f01acu, 0x68f6u); // mov.l	@r15+,r8 
gdxsv_WriteMem16(0x8c4f01aeu, 0xd014u); // mov.l	c4f0200 <gdx_softreset_disconnect+0x6c>,r0	! c045c68 
gdxsv_WriteMem16(0x8c4f01b0u, 0x400bu); // jsr	@r0 
gdxsv_WriteMem16(0x8c4f01b2u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f01b4u, 0x6203u); // mov	r0,r2 
gdxsv_WriteMem16(0x8c4f01b6u, 0x6082u); // mov.l	@r8,r0 
gdxsv_WriteMem16(0x8c4f01b8u, 0x8802u); // cmp/eq	#2,r0 
gdxsv_WriteMem16(0x8c4f01bau, 0x8f05u); // bf.s	c4f01c8 <gdx_softreset_disconnect+0x34> 
gdxsv_WriteMem16(0x8c4f01bcu, 0x6023u); // mov	r2,r0 
gdxsv_WriteMem16(0x8c4f01beu, 0xd011u); // mov.l	c4f0204 <gdx_softreset_disconnect+0x70>,r0	! c03308c 
gdxsv_WriteMem16(0x8c4f01c0u, 0x400bu); // jsr	@r0 
gdxsv_WriteMem16(0x8c4f01c2u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f01c4u, 0x6203u); // mov	r0,r2 
gdxsv_WriteMem16(0x8c4f01c6u, 0x6023u); // mov	r2,r0 
gdxsv_WriteMem16(0x8c4f01c8u, 0x8801u); // cmp/eq	#1,r0 
gdxsv_WriteMem16(0x8c4f01cau, 0x8fedu); // bf.s	c4f01a8 <gdx_softreset_disconnect+0x14> 
gdxsv_WriteMem16(0x8c4f01ccu, 0x6023u); // mov	r2,r0 
gdxsv_WriteMem16(0x8c4f01ceu, 0xd10eu); // mov.l	c4f0208 <gdx_softreset_disconnect+0x74>,r1	! c4e0650 <is_online> 
gdxsv_WriteMem16(0x8c4f01d0u, 0x6112u); // mov.l	@r1,r1 
gdxsv_WriteMem16(0x8c4f01d2u, 0x2118u); // tst	r1,r1 
gdxsv_WriteMem16(0x8c4f01d4u, 0x8de8u); // bt.s	c4f01a8 <gdx_softreset_disconnect+0x14> 
gdxsv_WriteMem16(0x8c4f01d6u, 0xe302u); // mov	#2,r3 
gdxsv_WriteMem16(0x8c4f01d8u, 0xd10cu); // mov.l	c4f020c <gdx_softreset_disconnect+0x78>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f01dau, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f01dcu, 0x2132u); // mov.l	r3,@r1 
gdxsv_WriteMem16(0x8c4f01deu, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f01e0u, 0x1122u); // mov.l	r2,@(8,r1) 
gdxsv_WriteMem16(0x8c4f01e2u, 0x1133u); // mov.l	r3,@(12,r1) 
gdxsv_WriteMem16(0x8c4f01e4u, 0x1124u); // mov.l	r2,@(16,r1) 
gdxsv_WriteMem16(0x8c4f01e6u, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f01e8u, 0xd209u); // mov.l	c4f0210 <gdx_softreset_disconnect+0x7c>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f01eau, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f01ecu, 0xd209u); // mov.l	c4f0214 <gdx_softreset_disconnect+0x80>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f01eeu, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f01f0u, 0x5211u); // mov.l	@(4,r1),r2 
gdxsv_WriteMem16(0x8c4f01f2u, 0x6023u); // mov	r2,r0 
gdxsv_WriteMem16(0x8c4f01f4u, 0x4f26u); // lds.l	@r15+,pr 
gdxsv_WriteMem16(0x8c4f01f6u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f01f8u, 0x68f6u); // mov.l	@r15+,r8 
gdxsv_WriteMem16(0x8c4f01fau, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f01fcu, 0x0654u); // mov.b	r5,@(r0,r6) 
gdxsv_WriteMem16(0x8c4f01feu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0200u, 0x5c68u); // mov.l	@(32,r6),r12 
gdxsv_WriteMem16(0x8c4f0202u, 0x0c04u); // mov.b	r0,@(r0,r12) 
gdxsv_WriteMem16(0x8c4f0204u, 0x308cu); // add	r8,r0 
gdxsv_WriteMem16(0x8c4f0206u, 0x0c03u); // bsrf	r12 
gdxsv_WriteMem16(0x8c4f0208u, 0x0650u); // .word 0x0650 
gdxsv_WriteMem16(0x8c4f020au, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f020cu, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f020eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0210u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f0212u, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f0214u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f0216u, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_ppp_get_status"] = 0x0c4f0218;
gdxsv_WriteMem16(0x8c4f0218u, 0xe201u); // mov	#1,r2 
gdxsv_WriteMem16(0x8c4f021au, 0x3426u); // cmp/hi	r2,r4 
gdxsv_WriteMem16(0x8c4f021cu, 0x8f19u); // bf.s	c4f0252 <gdx_ppp_get_status+0x3a> 
gdxsv_WriteMem16(0x8c4f021eu, 0x4f22u); // sts.l	pr,@-r15 
gdxsv_WriteMem16(0x8c4f0220u, 0x74feu); // add	#-2,r4 
gdxsv_WriteMem16(0x8c4f0222u, 0x3426u); // cmp/hi	r2,r4 
gdxsv_WriteMem16(0x8c4f0224u, 0x8d12u); // bt.s	c4f024c <gdx_ppp_get_status+0x34> 
gdxsv_WriteMem16(0x8c4f0226u, 0xe09du); // mov	#-99,r0 
gdxsv_WriteMem16(0x8c4f0228u, 0xd324u); // mov.l	c4f02bc <gdx_ppp_get_status+0xa4>,r3	! c4e0638 <ppp_status_ok> 
gdxsv_WriteMem16(0x8c4f022au, 0xe113u); // mov	#19,r1 
gdxsv_WriteMem16(0x8c4f022cu, 0x6263u); // mov	r6,r2 
gdxsv_WriteMem16(0x8c4f022eu, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0230u, 0x6734u); // mov.b	@r3+,r7 
gdxsv_WriteMem16(0x8c4f0232u, 0x4110u); // dt	r1 
gdxsv_WriteMem16(0x8c4f0234u, 0x2270u); // mov.b	r7,@r2 
gdxsv_WriteMem16(0x8c4f0236u, 0x8ffbu); // bf.s	c4f0230 <gdx_ppp_get_status+0x18> 
gdxsv_WriteMem16(0x8c4f0238u, 0x7201u); // add	#1,r2 
gdxsv_WriteMem16(0x8c4f023au, 0xd121u); // mov.l	c4f02c0 <gdx_ppp_get_status+0xa8>,r1	! c4e0654 <disk> 
gdxsv_WriteMem16(0x8c4f023cu, 0x6312u); // mov.l	@r1,r3 
gdxsv_WriteMem16(0x8c4f023eu, 0x6033u); // mov	r3,r0 
gdxsv_WriteMem16(0x8c4f0240u, 0x8801u); // cmp/eq	#1,r0 
gdxsv_WriteMem16(0x8c4f0242u, 0x8924u); // bt	c4f028e <gdx_ppp_get_status+0x76> 
gdxsv_WriteMem16(0x8c4f0244u, 0x6033u); // mov	r3,r0 
gdxsv_WriteMem16(0x8c4f0246u, 0x8802u); // cmp/eq	#2,r0 
gdxsv_WriteMem16(0x8c4f0248u, 0x8915u); // bt	c4f0276 <gdx_ppp_get_status+0x5e> 
gdxsv_WriteMem16(0x8c4f024au, 0xe000u); // mov	#0,r0 
gdxsv_WriteMem16(0x8c4f024cu, 0x4f26u); // lds.l	@r15+,pr 
gdxsv_WriteMem16(0x8c4f024eu, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0250u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0252u, 0xd11cu); // mov.l	c4f02c4 <gdx_ppp_get_status+0xac>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f0254u, 0xe302u); // mov	#2,r3 
gdxsv_WriteMem16(0x8c4f0256u, 0xe000u); // mov	#0,r0 
gdxsv_WriteMem16(0x8c4f0258u, 0x2132u); // mov.l	r3,@r1 
gdxsv_WriteMem16(0x8c4f025au, 0xe300u); // mov	#0,r3 
gdxsv_WriteMem16(0x8c4f025cu, 0x1131u); // mov.l	r3,@(4,r1) 
gdxsv_WriteMem16(0x8c4f025eu, 0x1142u); // mov.l	r4,@(8,r1) 
gdxsv_WriteMem16(0x8c4f0260u, 0x1123u); // mov.l	r2,@(12,r1) 
gdxsv_WriteMem16(0x8c4f0262u, 0x1134u); // mov.l	r3,@(16,r1) 
gdxsv_WriteMem16(0x8c4f0264u, 0xd218u); // mov.l	c4f02c8 <gdx_ppp_get_status+0xb0>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f0266u, 0x1135u); // mov.l	r3,@(20,r1) 
gdxsv_WriteMem16(0x8c4f0268u, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f026au, 0xd218u); // mov.l	c4f02cc <gdx_ppp_get_status+0xb4>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f026cu, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f026eu, 0x5111u); // mov.l	@(4,r1),r1 
gdxsv_WriteMem16(0x8c4f0270u, 0x4f26u); // lds.l	@r15+,pr 
gdxsv_WriteMem16(0x8c4f0272u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0274u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0276u, 0xd116u); // mov.l	c4f02d0 <gdx_ppp_get_status+0xb8>,r1	! c391d79 
gdxsv_WriteMem16(0x8c4f0278u, 0x6010u); // mov.b	@r1,r0 
gdxsv_WriteMem16(0x8c4f027au, 0x8810u); // cmp/eq	#16,r0 
gdxsv_WriteMem16(0x8c4f027cu, 0x8b00u); // bf	c4f0280 <gdx_ppp_get_status+0x68> 
gdxsv_WriteMem16(0x8c4f027eu, 0x2130u); // mov.b	r3,@r1 
gdxsv_WriteMem16(0x8c4f0280u, 0xd114u); // mov.l	c4f02d4 <gdx_ppp_get_status+0xbc>,r1	! c391d81 
gdxsv_WriteMem16(0x8c4f0282u, 0x6110u); // mov.b	@r1,r1 
gdxsv_WriteMem16(0x8c4f0284u, 0x2118u); // tst	r1,r1 
gdxsv_WriteMem16(0x8c4f0286u, 0x89e0u); // bt	c4f024a <gdx_ppp_get_status+0x32> 
gdxsv_WriteMem16(0x8c4f0288u, 0xe000u); // mov	#0,r0 
gdxsv_WriteMem16(0x8c4f028au, 0xafdfu); // bra	c4f024c <gdx_ppp_get_status+0x34> 
gdxsv_WriteMem16(0x8c4f028cu, 0x8068u); // mov.b	r0,@(8,r6) 
gdxsv_WriteMem16(0x8c4f028eu, 0xd212u); // mov.l	c4f02d8 <gdx_ppp_get_status+0xc0>,r2	! c2f6639 
gdxsv_WriteMem16(0x8c4f0290u, 0x6020u); // mov.b	@r2,r0 
gdxsv_WriteMem16(0x8c4f0292u, 0x8810u); // cmp/eq	#16,r0 
gdxsv_WriteMem16(0x8c4f0294u, 0x8d07u); // bt.s	c4f02a6 <gdx_ppp_get_status+0x8e> 
gdxsv_WriteMem16(0x8c4f0296u, 0xe302u); // mov	#2,r3 
gdxsv_WriteMem16(0x8c4f0298u, 0xd110u); // mov.l	c4f02dc <gdx_ppp_get_status+0xc4>,r1	! c2f6641 
gdxsv_WriteMem16(0x8c4f029au, 0x6110u); // mov.b	@r1,r1 
gdxsv_WriteMem16(0x8c4f029cu, 0x2118u); // tst	r1,r1 
gdxsv_WriteMem16(0x8c4f029eu, 0x89d4u); // bt	c4f024a <gdx_ppp_get_status+0x32> 
gdxsv_WriteMem16(0x8c4f02a0u, 0xe000u); // mov	#0,r0 
gdxsv_WriteMem16(0x8c4f02a2u, 0xafd3u); // bra	c4f024c <gdx_ppp_get_status+0x34> 
gdxsv_WriteMem16(0x8c4f02a4u, 0x8068u); // mov.b	r0,@(8,r6) 
gdxsv_WriteMem16(0x8c4f02a6u, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f02a8u, 0x7208u); // add	#8,r2 
gdxsv_WriteMem16(0x8c4f02aau, 0x6220u); // mov.b	@r2,r2 
gdxsv_WriteMem16(0x8c4f02acu, 0x2228u); // tst	r2,r2 
gdxsv_WriteMem16(0x8c4f02aeu, 0x8dc9u); // bt.s	c4f0244 <gdx_ppp_get_status+0x2c> 
gdxsv_WriteMem16(0x8c4f02b0u, 0x6312u); // mov.l	@r1,r3 
gdxsv_WriteMem16(0x8c4f02b2u, 0xe100u); // mov	#0,r1 
gdxsv_WriteMem16(0x8c4f02b4u, 0x6013u); // mov	r1,r0 
gdxsv_WriteMem16(0x8c4f02b6u, 0xafc5u); // bra	c4f0244 <gdx_ppp_get_status+0x2c> 
gdxsv_WriteMem16(0x8c4f02b8u, 0x8068u); // mov.b	r0,@(8,r6) 
gdxsv_WriteMem16(0x8c4f02bau, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f02bcu, 0x0638u); // .word 0x0638 
gdxsv_WriteMem16(0x8c4f02beu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f02c0u, 0x0654u); // mov.b	r5,@(r0,r6) 
gdxsv_WriteMem16(0x8c4f02c2u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f02c4u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f02c6u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f02c8u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f02cau, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f02ccu, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f02ceu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f02d0u, 0x1d79u); // mov.l	r7,@(36,r13) 
gdxsv_WriteMem16(0x8c4f02d2u, 0x0c39u); // .word 0x0c39 
gdxsv_WriteMem16(0x8c4f02d4u, 0x1d81u); // mov.l	r8,@(4,r13) 
gdxsv_WriteMem16(0x8c4f02d6u, 0x0c39u); // .word 0x0c39 
gdxsv_WriteMem16(0x8c4f02d8u, 0x6639u); // swap.w	r3,r6 
gdxsv_WriteMem16(0x8c4f02dau, 0x0c2fu); // mac.l	@r2+,@r12+ 
gdxsv_WriteMem16(0x8c4f02dcu, 0x6641u); // mov.w	@r4,r6 
gdxsv_WriteMem16(0x8c4f02deu, 0x0c2fu); // mac.l	@r2+,@r12+ 
symbols_["gdx_printf"] = 0x0c4f02e0;
gdxsv_WriteMem16(0x8c4f02e0u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f02e2u, 0x0009u); // nop 
symbols_["gdx_rpc_call"] = 0x0c4f02e4;
gdxsv_WriteMem16(0x8c4f02e4u, 0xd107u); // mov.l	c4f0304 <gdx_rpc_call+0x20>,r1	! c4e0200 <gdx_rpc> 
gdxsv_WriteMem16(0x8c4f02e6u, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f02e8u, 0x2142u); // mov.l	r4,@r1 
gdxsv_WriteMem16(0x8c4f02eau, 0x1121u); // mov.l	r2,@(4,r1) 
gdxsv_WriteMem16(0x8c4f02ecu, 0x1152u); // mov.l	r5,@(8,r1) 
gdxsv_WriteMem16(0x8c4f02eeu, 0x1163u); // mov.l	r6,@(12,r1) 
gdxsv_WriteMem16(0x8c4f02f0u, 0x62f2u); // mov.l	@r15,r2 
gdxsv_WriteMem16(0x8c4f02f2u, 0x1174u); // mov.l	r7,@(16,r1) 
gdxsv_WriteMem16(0x8c4f02f4u, 0x1125u); // mov.l	r2,@(20,r1) 
gdxsv_WriteMem16(0x8c4f02f6u, 0xd204u); // mov.l	c4f0308 <gdx_rpc_call+0x24>,r2	! 400000 
gdxsv_WriteMem16(0x8c4f02f8u, 0x6320u); // mov.b	@r2,r3 
gdxsv_WriteMem16(0x8c4f02fau, 0xd204u); // mov.l	c4f030c <gdx_rpc_call+0x28>,r2	! c4e0218 <dummy> 
gdxsv_WriteMem16(0x8c4f02fcu, 0x2230u); // mov.b	r3,@r2 
gdxsv_WriteMem16(0x8c4f02feu, 0x5011u); // mov.l	@(4,r1),r0 
gdxsv_WriteMem16(0x8c4f0300u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0302u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0304u, 0x0200u); // .word 0x0200 
gdxsv_WriteMem16(0x8c4f0306u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0308u, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f030au, 0x0040u); // .word 0x0040 
gdxsv_WriteMem16(0x8c4f030cu, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f030eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
symbols_["gdx_initialize"] = 0x0c4f0310;
gdxsv_WriteMem16(0x8c4f0310u, 0xd74cu); // mov.l	c4f0444 <gdx_initialize+0x134>,r7	! c4e0654 <disk> 
gdxsv_WriteMem16(0x8c4f0312u, 0x6072u); // mov.l	@r7,r0 
gdxsv_WriteMem16(0x8c4f0314u, 0x2008u); // tst	r0,r0 
gdxsv_WriteMem16(0x8c4f0316u, 0x8d06u); // bt.s	c4f0326 <gdx_initialize+0x16> 
gdxsv_WriteMem16(0x8c4f0318u, 0x8801u); // cmp/eq	#1,r0 
gdxsv_WriteMem16(0x8c4f031au, 0xd14bu); // mov.l	c4f0448 <gdx_initialize+0x138>,r1	! c4e0650 <is_online> 
gdxsv_WriteMem16(0x8c4f031cu, 0xe200u); // mov	#0,r2 
gdxsv_WriteMem16(0x8c4f031eu, 0x8d49u); // bt.s	c4f03b4 <gdx_initialize+0xa4> 
gdxsv_WriteMem16(0x8c4f0320u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0322u, 0x8802u); // cmp/eq	#2,r0 
gdxsv_WriteMem16(0x8c4f0324u, 0x8901u); // bt	c4f032a <gdx_initialize+0x1a> 
gdxsv_WriteMem16(0x8c4f0326u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0328u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f032au, 0xd148u); // mov.l	c4f044c <gdx_initialize+0x13c>,r1	! c4f0000 <gdx_sock_create> 
gdxsv_WriteMem16(0x8c4f032cu, 0xe309u); // mov	#9,r3 
gdxsv_WriteMem16(0x8c4f032eu, 0xd248u); // mov.l	c4f0450 <gdx_initialize+0x140>,r2	! 8c0228f8 
gdxsv_WriteMem16(0x8c4f0330u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0332u, 0xd248u); // mov.l	c4f0454 <gdx_initialize+0x144>,r2	! 8c045504 
gdxsv_WriteMem16(0x8c4f0334u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0336u, 0xd148u); // mov.l	c4f0458 <gdx_initialize+0x148>,r1	! c4f0004 <gdx_sock_close> 
gdxsv_WriteMem16(0x8c4f0338u, 0xd248u); // mov.l	c4f045c <gdx_initialize+0x14c>,r2	! 8c01f284 
gdxsv_WriteMem16(0x8c4f033au, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f033cu, 0xd248u); // mov.l	c4f0460 <gdx_initialize+0x150>,r2	! 8c0228dc 
gdxsv_WriteMem16(0x8c4f033eu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0340u, 0xd248u); // mov.l	c4f0464 <gdx_initialize+0x154>,r2	! 8c022a08 
gdxsv_WriteMem16(0x8c4f0342u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0344u, 0xd248u); // mov.l	c4f0468 <gdx_initialize+0x158>,r2	! 8c022b28 
gdxsv_WriteMem16(0x8c4f0346u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0348u, 0xd248u); // mov.l	c4f046c <gdx_initialize+0x15c>,r2	! 8c022d3c 
gdxsv_WriteMem16(0x8c4f034au, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f034cu, 0xd248u); // mov.l	c4f0470 <gdx_initialize+0x160>,r2	! 8c03322c 
gdxsv_WriteMem16(0x8c4f034eu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0350u, 0xd248u); // mov.l	c4f0474 <gdx_initialize+0x164>,r2	! 8c045510 
gdxsv_WriteMem16(0x8c4f0352u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0354u, 0xd248u); // mov.l	c4f0478 <gdx_initialize+0x168>,r2	! 8c045824 
gdxsv_WriteMem16(0x8c4f0356u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0358u, 0xd148u); // mov.l	c4f047c <gdx_initialize+0x16c>,r1	! 8c022908 
gdxsv_WriteMem16(0x8c4f035au, 0xd249u); // mov.l	c4f0480 <gdx_initialize+0x170>,r2	! c4f003c <gdx_gethostbyname> 
gdxsv_WriteMem16(0x8c4f035cu, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f035eu, 0xd149u); // mov.l	c4f0484 <gdx_initialize+0x174>,r1	! c4f0068 <gdx_connect_sock> 
gdxsv_WriteMem16(0x8c4f0360u, 0xd249u); // mov.l	c4f0488 <gdx_initialize+0x178>,r2	! 8c022a04 
gdxsv_WriteMem16(0x8c4f0362u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0364u, 0xd249u); // mov.l	c4f048c <gdx_initialize+0x17c>,r2	! 8c04550c 
gdxsv_WriteMem16(0x8c4f0366u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0368u, 0xd149u); // mov.l	c4f0490 <gdx_initialize+0x180>,r1	! c4f00b4 <gdx_select> 
gdxsv_WriteMem16(0x8c4f036au, 0xd24au); // mov.l	c4f0494 <gdx_initialize+0x184>,r2	! 8c033ec0 
gdxsv_WriteMem16(0x8c4f036cu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f036eu, 0xd24au); // mov.l	c4f0498 <gdx_initialize+0x188>,r2	! 8c045818 
gdxsv_WriteMem16(0x8c4f0370u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0372u, 0xd14au); // mov.l	c4f049c <gdx_initialize+0x18c>,r1	! 8c1a9878 
gdxsv_WriteMem16(0x8c4f0374u, 0xd24au); // mov.l	c4f04a0 <gdx_initialize+0x190>,r2	! c4f0218 <gdx_ppp_get_status> 
gdxsv_WriteMem16(0x8c4f0376u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0378u, 0xd14au); // mov.l	c4f04a4 <gdx_initialize+0x194>,r1	! 8c033fd8 
gdxsv_WriteMem16(0x8c4f037au, 0xd24bu); // mov.l	c4f04a8 <gdx_initialize+0x198>,r2	! c4f0120 <gdx_lbs_sock_read> 
gdxsv_WriteMem16(0x8c4f037cu, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f037eu, 0xd14bu); // mov.l	c4f04ac <gdx_initialize+0x19c>,r1	! 8c034464 
gdxsv_WriteMem16(0x8c4f0380u, 0xd24bu); // mov.l	c4f04b0 <gdx_initialize+0x1a0>,r2	! c4f00f4 <gdx_lbs_sock_write> 
gdxsv_WriteMem16(0x8c4f0382u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0384u, 0xd14bu); // mov.l	c4f04b4 <gdx_initialize+0x1a4>,r1	! 8c046678 
gdxsv_WriteMem16(0x8c4f0386u, 0xd24cu); // mov.l	c4f04b8 <gdx_initialize+0x1a8>,r2	! c4f015c <gdx_mcs_sock_read> 
gdxsv_WriteMem16(0x8c4f0388u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f038au, 0xd14cu); // mov.l	c4f04bc <gdx_initialize+0x1ac>,r1	! 8c045820 
gdxsv_WriteMem16(0x8c4f038cu, 0xd24cu); // mov.l	c4f04c0 <gdx_initialize+0x1b0>,r2	! c4f0168 <gdx_mcs_sock_write> 
gdxsv_WriteMem16(0x8c4f038eu, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0390u, 0xd14cu); // mov.l	c4f04c4 <gdx_initialize+0x1b4>,r1	! c4f0194 <gdx_softreset_disconnect> 
gdxsv_WriteMem16(0x8c4f0392u, 0xd24du); // mov.l	c4f04c8 <gdx_initialize+0x1b8>,r2	! 8c010a54 
gdxsv_WriteMem16(0x8c4f0394u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0396u, 0xd24du); // mov.l	c4f04cc <gdx_initialize+0x1bc>,r2	! 8c010e04 
gdxsv_WriteMem16(0x8c4f0398u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f039au, 0xd24du); // mov.l	c4f04d0 <gdx_initialize+0x1c0>,r2	! 8c011038 
gdxsv_WriteMem16(0x8c4f039cu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f039eu, 0xd24du); // mov.l	c4f04d4 <gdx_initialize+0x1c4>,r2	! 8c052134 
gdxsv_WriteMem16(0x8c4f03a0u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03a2u, 0xe10cu); // mov	#12,r1 
gdxsv_WriteMem16(0x8c4f03a4u, 0xd24cu); // mov.l	c4f04d8 <gdx_initialize+0x1c8>,r2	! 8c022818 
gdxsv_WriteMem16(0x8c4f03a6u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f03a8u, 0x2231u); // mov.w	r3,@r2 
gdxsv_WriteMem16(0x8c4f03aau, 0x4110u); // dt	r1 
gdxsv_WriteMem16(0x8c4f03acu, 0x8ffcu); // bf.s	c4f03a8 <gdx_initialize+0x98> 
gdxsv_WriteMem16(0x8c4f03aeu, 0x7202u); // add	#2,r2 
gdxsv_WriteMem16(0x8c4f03b0u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f03b2u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f03b4u, 0xd125u); // mov.l	c4f044c <gdx_initialize+0x13c>,r1	! c4f0000 <gdx_sock_create> 
gdxsv_WriteMem16(0x8c4f03b6u, 0xe309u); // mov	#9,r3 
gdxsv_WriteMem16(0x8c4f03b8u, 0xd248u); // mov.l	c4f04dc <gdx_initialize+0x1cc>,r2	! 8c05811c 
gdxsv_WriteMem16(0x8c4f03bau, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03bcu, 0xd248u); // mov.l	c4f04e0 <gdx_initialize+0x1d0>,r2	! 8c0354d4 
gdxsv_WriteMem16(0x8c4f03beu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03c0u, 0xd125u); // mov.l	c4f0458 <gdx_initialize+0x148>,r1	! c4f0004 <gdx_sock_close> 
gdxsv_WriteMem16(0x8c4f03c2u, 0xd248u); // mov.l	c4f04e4 <gdx_initialize+0x1d4>,r2	! 8c05843c 
gdxsv_WriteMem16(0x8c4f03c4u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03c6u, 0xd248u); // mov.l	c4f04e8 <gdx_initialize+0x1d8>,r2	! 8c058128 
gdxsv_WriteMem16(0x8c4f03c8u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03cau, 0xd248u); // mov.l	c4f04ec <gdx_initialize+0x1dc>,r2	! 8c045e08 
gdxsv_WriteMem16(0x8c4f03ccu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03ceu, 0xd248u); // mov.l	c4f04f0 <gdx_initialize+0x1e0>,r2	! 8c035918 
gdxsv_WriteMem16(0x8c4f03d0u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03d2u, 0xd248u); // mov.l	c4f04f4 <gdx_initialize+0x1e4>,r2	! 8c035704 
gdxsv_WriteMem16(0x8c4f03d4u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03d6u, 0xd248u); // mov.l	c4f04f8 <gdx_initialize+0x1e8>,r2	! 8c0355e4 
gdxsv_WriteMem16(0x8c4f03d8u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03dau, 0xd248u); // mov.l	c4f04fc <gdx_initialize+0x1ec>,r2	! 8c0354b8 
gdxsv_WriteMem16(0x8c4f03dcu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03deu, 0xd248u); // mov.l	c4f0500 <gdx_initialize+0x1f0>,r2	! 8c031efc 
gdxsv_WriteMem16(0x8c4f03e0u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03e2u, 0xd148u); // mov.l	c4f0504 <gdx_initialize+0x1f4>,r1	! 8c0354e4 
gdxsv_WriteMem16(0x8c4f03e4u, 0xd226u); // mov.l	c4f0480 <gdx_initialize+0x170>,r2	! c4f003c <gdx_gethostbyname> 
gdxsv_WriteMem16(0x8c4f03e6u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f03e8u, 0xd126u); // mov.l	c4f0484 <gdx_initialize+0x174>,r1	! c4f0068 <gdx_connect_sock> 
gdxsv_WriteMem16(0x8c4f03eau, 0xd247u); // mov.l	c4f0508 <gdx_initialize+0x1f8>,r2	! 8c058124 
gdxsv_WriteMem16(0x8c4f03ecu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03eeu, 0xd247u); // mov.l	c4f050c <gdx_initialize+0x1fc>,r2	! 8c0355e0 
gdxsv_WriteMem16(0x8c4f03f0u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03f2u, 0xd127u); // mov.l	c4f0490 <gdx_initialize+0x180>,r1	! c4f00b4 <gdx_select> 
gdxsv_WriteMem16(0x8c4f03f4u, 0xd246u); // mov.l	c4f0510 <gdx_initialize+0x200>,r2	! 8c058430 
gdxsv_WriteMem16(0x8c4f03f6u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03f8u, 0xd246u); // mov.l	c4f0514 <gdx_initialize+0x204>,r2	! 8c046a9c 
gdxsv_WriteMem16(0x8c4f03fau, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f03fcu, 0xd146u); // mov.l	c4f0518 <gdx_initialize+0x208>,r1	! 8c14a098 
gdxsv_WriteMem16(0x8c4f03feu, 0xd228u); // mov.l	c4f04a0 <gdx_initialize+0x190>,r2	! c4f0218 <gdx_ppp_get_status> 
gdxsv_WriteMem16(0x8c4f0400u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0402u, 0xd146u); // mov.l	c4f051c <gdx_initialize+0x20c>,r1	! 8c046bb4 
gdxsv_WriteMem16(0x8c4f0404u, 0xd228u); // mov.l	c4f04a8 <gdx_initialize+0x198>,r2	! c4f0120 <gdx_lbs_sock_read> 
gdxsv_WriteMem16(0x8c4f0406u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0408u, 0xd145u); // mov.l	c4f0520 <gdx_initialize+0x210>,r1	! 8c047040 
gdxsv_WriteMem16(0x8c4f040au, 0xd229u); // mov.l	c4f04b0 <gdx_initialize+0x1a0>,r2	! c4f00f4 <gdx_lbs_sock_write> 
gdxsv_WriteMem16(0x8c4f040cu, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f040eu, 0xd145u); // mov.l	c4f0524 <gdx_initialize+0x214>,r1	! 8c059290 
gdxsv_WriteMem16(0x8c4f0410u, 0xd229u); // mov.l	c4f04b8 <gdx_initialize+0x1a8>,r2	! c4f015c <gdx_mcs_sock_read> 
gdxsv_WriteMem16(0x8c4f0412u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0414u, 0xd144u); // mov.l	c4f0528 <gdx_initialize+0x218>,r1	! 8c058438 
gdxsv_WriteMem16(0x8c4f0416u, 0xd22au); // mov.l	c4f04c0 <gdx_initialize+0x1b0>,r2	! c4f0168 <gdx_mcs_sock_write> 
gdxsv_WriteMem16(0x8c4f0418u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f041au, 0xd12au); // mov.l	c4f04c4 <gdx_initialize+0x1b4>,r1	! c4f0194 <gdx_softreset_disconnect> 
gdxsv_WriteMem16(0x8c4f041cu, 0xd243u); // mov.l	c4f052c <gdx_initialize+0x21c>,r2	! 8c010a4c 
gdxsv_WriteMem16(0x8c4f041eu, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0420u, 0xd243u); // mov.l	c4f0530 <gdx_initialize+0x220>,r2	! 8c010e00 
gdxsv_WriteMem16(0x8c4f0422u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0424u, 0xd243u); // mov.l	c4f0534 <gdx_initialize+0x224>,r2	! 8c01101c 
gdxsv_WriteMem16(0x8c4f0426u, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f0428u, 0xd243u); // mov.l	c4f0538 <gdx_initialize+0x228>,r2	! 8c064d20 
gdxsv_WriteMem16(0x8c4f042au, 0x2212u); // mov.l	r1,@r2 
gdxsv_WriteMem16(0x8c4f042cu, 0xe10cu); // mov	#12,r1 
gdxsv_WriteMem16(0x8c4f042eu, 0xd243u); // mov.l	c4f053c <gdx_initialize+0x22c>,r2	! 8c0353f4 
gdxsv_WriteMem16(0x8c4f0430u, 0x2231u); // mov.w	r3,@r2 
gdxsv_WriteMem16(0x8c4f0432u, 0x4110u); // dt	r1 
gdxsv_WriteMem16(0x8c4f0434u, 0x8ffcu); // bf.s	c4f0430 <gdx_initialize+0x120> 
gdxsv_WriteMem16(0x8c4f0436u, 0x7202u); // add	#2,r2 
gdxsv_WriteMem16(0x8c4f0438u, 0x6072u); // mov.l	@r7,r0 
gdxsv_WriteMem16(0x8c4f043au, 0x8802u); // cmp/eq	#2,r0 
gdxsv_WriteMem16(0x8c4f043cu, 0x8bb8u); // bf	c4f03b0 <gdx_initialize+0xa0> 
gdxsv_WriteMem16(0x8c4f043eu, 0xaf74u); // bra	c4f032a <gdx_initialize+0x1a> 
gdxsv_WriteMem16(0x8c4f0440u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0442u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0444u, 0x0654u); // mov.b	r5,@(r0,r6) 
gdxsv_WriteMem16(0x8c4f0446u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0448u, 0x0650u); // .word 0x0650 
gdxsv_WriteMem16(0x8c4f044au, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f044cu, 0x0000u); // .word 0x0000 
gdxsv_WriteMem16(0x8c4f044eu, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f0450u, 0x28f8u); // tst	r15,r8 
gdxsv_WriteMem16(0x8c4f0452u, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f0454u, 0x5504u); // mov.l	@(16,r0),r5 
gdxsv_WriteMem16(0x8c4f0456u, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f0458u, 0x0004u); // mov.b	r0,@(r0,r0) 
gdxsv_WriteMem16(0x8c4f045au, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f045cu, 0xf284u); // .word 0xf284 
gdxsv_WriteMem16(0x8c4f045eu, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f0460u, 0x28dcu); // cmp/str	r13,r8 
gdxsv_WriteMem16(0x8c4f0462u, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f0464u, 0x2a08u); // tst	r0,r10 
gdxsv_WriteMem16(0x8c4f0466u, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f0468u, 0x2b28u); // tst	r2,r11 
gdxsv_WriteMem16(0x8c4f046au, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f046cu, 0x2d3cu); // cmp/str	r3,r13 
gdxsv_WriteMem16(0x8c4f046eu, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f0470u, 0x322cu); // add	r2,r2 
gdxsv_WriteMem16(0x8c4f0472u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f0474u, 0x5510u); // mov.l	@(0,r1),r5 
gdxsv_WriteMem16(0x8c4f0476u, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f0478u, 0x5824u); // mov.l	@(16,r2),r8 
gdxsv_WriteMem16(0x8c4f047au, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f047cu, 0x2908u); // tst	r0,r9 
gdxsv_WriteMem16(0x8c4f047eu, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f0480u, 0x003cu); // mov.b	@(r0,r3),r0 
gdxsv_WriteMem16(0x8c4f0482u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f0484u, 0x0068u); // .word 0x0068 
gdxsv_WriteMem16(0x8c4f0486u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f0488u, 0x2a04u); // mov.b	r0,@-r10 
gdxsv_WriteMem16(0x8c4f048au, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f048cu, 0x550cu); // mov.l	@(48,r0),r5 
gdxsv_WriteMem16(0x8c4f048eu, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f0490u, 0x00b4u); // mov.b	r11,@(r0,r0) 
gdxsv_WriteMem16(0x8c4f0492u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f0494u, 0x3ec0u); // cmp/eq	r12,r14 
gdxsv_WriteMem16(0x8c4f0496u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f0498u, 0x5818u); // mov.l	@(32,r1),r8 
gdxsv_WriteMem16(0x8c4f049au, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f049cu, 0x9878u); // mov.w	c4f0590 <gdx_dial_start_disk2+0x28>,r8 
gdxsv_WriteMem16(0x8c4f049eu, 0x8c1au); // .word 0x8c1a 
gdxsv_WriteMem16(0x8c4f04a0u, 0x0218u); // .word 0x0218 
gdxsv_WriteMem16(0x8c4f04a2u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f04a4u, 0x3fd8u); // sub	r13,r15 
gdxsv_WriteMem16(0x8c4f04a6u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f04a8u, 0x0120u); // .word 0x0120 
gdxsv_WriteMem16(0x8c4f04aau, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f04acu, 0x4464u); // .word 0x4464 
gdxsv_WriteMem16(0x8c4f04aeu, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f04b0u, 0x00f4u); // mov.b	r15,@(r0,r0) 
gdxsv_WriteMem16(0x8c4f04b2u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f04b4u, 0x6678u); // swap.b	r7,r6 
gdxsv_WriteMem16(0x8c4f04b6u, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f04b8u, 0x015cu); // mov.b	@(r0,r5),r1 
gdxsv_WriteMem16(0x8c4f04bau, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f04bcu, 0x5820u); // mov.l	@(0,r2),r8 
gdxsv_WriteMem16(0x8c4f04beu, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f04c0u, 0x0168u); // .word 0x0168 
gdxsv_WriteMem16(0x8c4f04c2u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f04c4u, 0x0194u); // mov.b	r9,@(r0,r1) 
gdxsv_WriteMem16(0x8c4f04c6u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f04c8u, 0x0a54u); // mov.b	r5,@(r0,r10) 
gdxsv_WriteMem16(0x8c4f04cau, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f04ccu, 0x0e04u); // mov.b	r0,@(r0,r14) 
gdxsv_WriteMem16(0x8c4f04ceu, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f04d0u, 0x1038u); // mov.l	r3,@(32,r0) 
gdxsv_WriteMem16(0x8c4f04d2u, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f04d4u, 0x2134u); // mov.b	r3,@-r1 
gdxsv_WriteMem16(0x8c4f04d6u, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f04d8u, 0x2818u); // tst	r1,r8 
gdxsv_WriteMem16(0x8c4f04dau, 0x8c02u); // .word 0x8c02 
gdxsv_WriteMem16(0x8c4f04dcu, 0x811cu); // mov.w	r0,@(24,r1) 
gdxsv_WriteMem16(0x8c4f04deu, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f04e0u, 0x54d4u); // mov.l	@(16,r13),r4 
gdxsv_WriteMem16(0x8c4f04e2u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f04e4u, 0x843cu); // mov.b	@(12,r3),r0 
gdxsv_WriteMem16(0x8c4f04e6u, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f04e8u, 0x8128u); // mov.w	r0,@(16,r2) 
gdxsv_WriteMem16(0x8c4f04eau, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f04ecu, 0x5e08u); // mov.l	@(32,r0),r14 
gdxsv_WriteMem16(0x8c4f04eeu, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f04f0u, 0x5918u); // mov.l	@(32,r1),r9 
gdxsv_WriteMem16(0x8c4f04f2u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f04f4u, 0x5704u); // mov.l	@(16,r0),r7 
gdxsv_WriteMem16(0x8c4f04f6u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f04f8u, 0x55e4u); // mov.l	@(16,r14),r5 
gdxsv_WriteMem16(0x8c4f04fau, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f04fcu, 0x54b8u); // mov.l	@(32,r11),r4 
gdxsv_WriteMem16(0x8c4f04feu, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f0500u, 0x1efcu); // mov.l	r15,@(48,r14) 
gdxsv_WriteMem16(0x8c4f0502u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f0504u, 0x54e4u); // mov.l	@(16,r14),r4 
gdxsv_WriteMem16(0x8c4f0506u, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f0508u, 0x8124u); // mov.w	r0,@(8,r2) 
gdxsv_WriteMem16(0x8c4f050au, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f050cu, 0x55e0u); // mov.l	@(0,r14),r5 
gdxsv_WriteMem16(0x8c4f050eu, 0x8c03u); // .word 0x8c03 
gdxsv_WriteMem16(0x8c4f0510u, 0x8430u); // mov.b	@(0,r3),r0 
gdxsv_WriteMem16(0x8c4f0512u, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f0514u, 0x6a9cu); // extu.b	r9,r10 
gdxsv_WriteMem16(0x8c4f0516u, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f0518u, 0xa098u); // bra	c4f064c <gdx_dial_start_disk2+0xe4> 
gdxsv_WriteMem16(0x8c4f051au, 0x8c14u); // .word 0x8c14 
gdxsv_WriteMem16(0x8c4f051cu, 0x6bb4u); // mov.b	@r11+,r11 
gdxsv_WriteMem16(0x8c4f051eu, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f0520u, 0x7040u); // add	#64,r0 
gdxsv_WriteMem16(0x8c4f0522u, 0x8c04u); // .word 0x8c04 
gdxsv_WriteMem16(0x8c4f0524u, 0x9290u); // mov.w	c4f0648 <gdx_dial_start_disk2+0xe0>,r2 
gdxsv_WriteMem16(0x8c4f0526u, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f0528u, 0x8438u); // mov.b	@(8,r3),r0 
gdxsv_WriteMem16(0x8c4f052au, 0x8c05u); // .word 0x8c05 
gdxsv_WriteMem16(0x8c4f052cu, 0x0a4cu); // mov.b	@(r0,r4),r10 
gdxsv_WriteMem16(0x8c4f052eu, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f0530u, 0x0e00u); // .word 0x0e00 
gdxsv_WriteMem16(0x8c4f0532u, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f0534u, 0x101cu); // mov.l	r1,@(48,r0) 
gdxsv_WriteMem16(0x8c4f0536u, 0x8c01u); // .word 0x8c01 
gdxsv_WriteMem16(0x8c4f0538u, 0x4d20u); // shal	r13 
gdxsv_WriteMem16(0x8c4f053au, 0x8c06u); // .word 0x8c06 
gdxsv_WriteMem16(0x8c4f053cu, 0x53f4u); // mov.l	@(16,r15),r3 
gdxsv_WriteMem16(0x8c4f053eu, 0x8c03u); // .word 0x8c03 
symbols_["gdx_dial_start_disk1"] = 0x0c4f0540;
gdxsv_WriteMem16(0x8c4f0540u, 0xd106u); // mov.l	c4f055c <gdx_dial_start_disk1+0x1c>,r1	! c4e0654 <disk> 
gdxsv_WriteMem16(0x8c4f0542u, 0xe201u); // mov	#1,r2 
gdxsv_WriteMem16(0x8c4f0544u, 0x4f22u); // sts.l	pr,@-r15 
gdxsv_WriteMem16(0x8c4f0546u, 0x2122u); // mov.l	r2,@r1 
gdxsv_WriteMem16(0x8c4f0548u, 0xd105u); // mov.l	c4f0560 <gdx_dial_start_disk1+0x20>,r1	! c4f0310 <gdx_initialize> 
gdxsv_WriteMem16(0x8c4f054au, 0x410bu); // jsr	@r1 
gdxsv_WriteMem16(0x8c4f054cu, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f054eu, 0xd105u); // mov.l	c4f0564 <gdx_dial_start_disk1+0x24>,r1	! c2f6639 
gdxsv_WriteMem16(0x8c4f0550u, 0xe202u); // mov	#2,r2 
gdxsv_WriteMem16(0x8c4f0552u, 0x2120u); // mov.b	r2,@r1 
gdxsv_WriteMem16(0x8c4f0554u, 0x4f26u); // lds.l	@r15+,pr 
gdxsv_WriteMem16(0x8c4f0556u, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0558u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f055au, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f055cu, 0x0654u); // mov.b	r5,@(r0,r6) 
gdxsv_WriteMem16(0x8c4f055eu, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0560u, 0x0310u); // .word 0x0310 
gdxsv_WriteMem16(0x8c4f0562u, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f0564u, 0x6639u); // swap.w	r3,r6 
gdxsv_WriteMem16(0x8c4f0566u, 0x0c2fu); // mac.l	@r2+,@r12+ 
symbols_["gdx_dial_start_disk2"] = 0x0c4f0568;
gdxsv_WriteMem16(0x8c4f0568u, 0x2f86u); // mov.l	r8,@-r15 
gdxsv_WriteMem16(0x8c4f056au, 0xe802u); // mov	#2,r8 
gdxsv_WriteMem16(0x8c4f056cu, 0xd105u); // mov.l	c4f0584 <gdx_dial_start_disk2+0x1c>,r1	! c4e0654 <disk> 
gdxsv_WriteMem16(0x8c4f056eu, 0x4f22u); // sts.l	pr,@-r15 
gdxsv_WriteMem16(0x8c4f0570u, 0x2182u); // mov.l	r8,@r1 
gdxsv_WriteMem16(0x8c4f0572u, 0xd105u); // mov.l	c4f0588 <gdx_dial_start_disk2+0x20>,r1	! c4f0310 <gdx_initialize> 
gdxsv_WriteMem16(0x8c4f0574u, 0x410bu); // jsr	@r1 
gdxsv_WriteMem16(0x8c4f0576u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0578u, 0xd104u); // mov.l	c4f058c <gdx_dial_start_disk2+0x24>,r1	! c391d79 
gdxsv_WriteMem16(0x8c4f057au, 0x2180u); // mov.b	r8,@r1 
gdxsv_WriteMem16(0x8c4f057cu, 0x4f26u); // lds.l	@r15+,pr 
gdxsv_WriteMem16(0x8c4f057eu, 0x000bu); // rts 
gdxsv_WriteMem16(0x8c4f0580u, 0x68f6u); // mov.l	@r15+,r8 
gdxsv_WriteMem16(0x8c4f0582u, 0x0009u); // nop 
gdxsv_WriteMem16(0x8c4f0584u, 0x0654u); // mov.b	r5,@(r0,r6) 
gdxsv_WriteMem16(0x8c4f0586u, 0x0c4eu); // mov.l	@(r0,r4),r12 
gdxsv_WriteMem16(0x8c4f0588u, 0x0310u); // .word 0x0310 
gdxsv_WriteMem16(0x8c4f058au, 0x0c4fu); // mac.l	@r4+,@r12+ 
gdxsv_WriteMem16(0x8c4f058cu, 0x1d79u); // mov.l	r7,@(36,r13) 
gdxsv_WriteMem16(0x8c4f058eu, 0x0c39u); // .word 0x0c39 
if (disk_ == 1) gdxsv_WriteMem32(0x8c181bb4, symbols_["gdx_dial_start_disk1"]);
if (disk_ == 2) gdxsv_WriteMem32(0x8c1e0274, symbols_["gdx_dial_start_disk2"]);
symbols_[":patch_id"] = 82631140;
gdxsv_WriteMem32(symbols_["patch_id"], symbols_[":patch_id"]);
