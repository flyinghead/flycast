UNAME=$(shell uname)
ifeq ($(findstring MINGW,$(UNAME)),)
	MOD_OBJ+=$(LIBBASE)modules/pico_dev_tun.o
endif
