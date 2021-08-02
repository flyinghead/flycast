PREFIX?=/usr/local
INSTALL_DIR=$(PREFIX)/include/xbyak

all:
	$(MAKE) -C sample

clean:
	$(MAKE) -C sample clean

install:
	mkdir -p $(INSTALL_DIR)
	cp -pR xbyak/*.h $(INSTALL_DIR)

uninstall:
	rm -i $(INSTALL_DIR)/*.h
	rmdir $(INSTALL_DIR)

update:
	$(MAKE) -C gen

test:
	$(MAKE) -C test test

.PHONY: test update
