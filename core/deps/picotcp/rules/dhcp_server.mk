OPTIONS+=-DPICO_SUPPORT_DHCPD
MOD_OBJ+=$(LIBBASE)modules/pico_dhcp_server.o $(LIBBASE)modules/pico_dhcp_common.o
