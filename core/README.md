# core/libdreamcast
===========

Here lies the core of our codebase. Everything that's OS inspecific rests here.
** Please check per directory README for more info **

### Some rudimentary categories are:
- hw				  -- DC Hardware Components Implementation
- nullDC.cpp	-- NullDC, thy mighty child (also referenced as "debugger")
- emitter			-- Cookie machine 
- khronos			-- Vulkan stuff
- oslib				-- Codebase abstraction effort
- cfg				  -- Configuration backend structure
- reios				-- (Our)Implementation of the DreamCast BIOS (Not functional)
- deps				-- External C libraries (hackish, hand-written versions)
