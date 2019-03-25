UNAME=$(shell uname)
ifeq ($(findstring MINGW,$(UNAME)),)
    MOD_OBJ+=$(LIBBASE)modules/pico_dev_tap.o
else
    MOD_OBJ+=$(LIBBASE)modules/pico_dev_tap_windows.o
endif
