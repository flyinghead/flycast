#pragma once

#include "types.h"

#define InetBufSize 1024

enum {
	GDX_RPC_SOCK_OPEN = 1,
	GDX_RPC_SOCK_CLOSE = 2,
	GDX_RPC_SOCK_READ = 3,
	GDX_RPC_SOCK_WRITE = 4,
	GDX_RPC_SOCK_POLL = 5,
};

struct gdx_rpc_t {
	u32 request;
	u32 response;
	u32 param1;
	u32 param2;
	u32 param3;
	u32 param4;
};