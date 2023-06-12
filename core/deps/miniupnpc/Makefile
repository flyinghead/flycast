# $Id: Makefile,v 1.145 2021/09/28 20:42:25 nanard Exp $
# MiniUPnP Project
# http://miniupnp.free.fr/
# https://miniupnp.tuxfamily.org/
# https://github.com/miniupnp/miniupnp
# (c) 2005-2022 Thomas Bernard
# to install use :
# $ make DESTDIR=/tmp/dummylocation install
# or
# $ INSTALLPREFIX=/usr/local make install
# or
# $ make install (default INSTALLPREFIX is /usr)
OS = $(shell $(CC) -dumpmachine)
VERSION = $(shell cat VERSION)

ifneq (, $(findstring darwin, $(OS)))
JARSUFFIX=mac
LIBTOOL ?= $(shell which libtool)
endif
ifneq (, $(findstring linux, $(OS)))
JARSUFFIX=linux
endif
ifneq (, $(findstring mingw, $(OS))$(findstring cygwin, $(OS))$(findstring msys, $(OS)))
JARSUFFIX=win32
endif

HAVE_IPV6 ?= yes
export HAVE_IPV6

# directories
INCDIR = include
SRCDIR = src
BUILD = build

CC ?= gcc
#AR = gar
#CFLAGS = -O -g
# to debug :
ASANFLAGS = -fsanitize=address -fsanitize=undefined -fsanitize=leak
#CFLAGS = -g -ggdb -O0 $(ASANFLAGS) -fno-omit-frame-pointer
#CPPFLAGS += -DDEBUG
#LDFLAGS += $(ASANFLAGS)
CFLAGS ?= -O
CFLAGS += -Wall
CFLAGS += -W -Wstrict-prototypes
CFLAGS += -fno-common
CPPFLAGS += -I$(BUILD)
CPPFLAGS += -DMINIUPNPC_SET_SOCKET_TIMEOUT
CPPFLAGS += -DMINIUPNPC_GET_SRC_ADDR
CPPFLAGS += -D_BSD_SOURCE
CPPFLAGS += -D_DEFAULT_SOURCE
ifneq (, $(findstring netbsd, $(OS)))
CPPFLAGS += -D_NETBSD_SOURCE
endif
ifeq (, $(findstring freebsd, $(OS))$(findstring darwin, $(OS)))
#CPPFLAGS += -D_POSIX_C_SOURCE=200112L
CPPFLAGS += -D_XOPEN_SOURCE=600
endif
#CFLAGS += -ansi
#CPPFLAGS += -DNO_GETADDRINFO

DEPFLAGS = -MM -MG

MKDIR = mkdir -p
INSTALL = install
SH = /bin/sh
JAVA = java
# see http://code.google.com/p/jnaerator/
#JNAERATOR = jnaerator-0.9.7.jar
#JNAERATOR = jnaerator-0.9.8-shaded.jar
#JNAERATORARGS = -library miniupnpc
#JNAERATOR = jnaerator-0.10-shaded.jar
#JNAERATOR = jnaerator-0.11-shaded.jar
# https://repo1.maven.org/maven2/com/nativelibs4java/jnaerator/0.12/jnaerator-0.12-shaded.jar
JNAERATOR = jnaerator-0.12-shaded.jar
JNAERATORARGS = -mode StandaloneJar -runtime JNAerator -library miniupnpc
#JNAERATORBASEURL = http://jnaerator.googlecode.com/files/
JNAERATORBASEURL = https://repo1.maven.org/maven2/com/nativelibs4java/jnaerator/0.12

ifneq (, $(findstring sun, $(OS))$(findstring solaris, $(OS)))
  LDLIBS=-lsocket -lnsl -lresolv
  CPPFLAGS += -D__EXTENSIONS__
  CFLAGS += -std=c99
endif

# APIVERSION is used to build SONAME
APIVERSION = 17

SRCS = $(wildcard $(SRCDIR)/*.c)

LIBOBJS = $(addprefix $(BUILD)/,miniwget.o minixml.o igd_desc_parse.o minisoap.o \
          miniupnpc.o upnpreplyparse.o upnpcommands.o upnperrors.o \
          connecthostport.o portlistingparse.o receivedata.o upnpdev.o \
          addr_is_reserved.o)

BUILDINCLUDES = $(addprefix $(BUILD)/, miniupnpcstrings.h)

OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.o,$(SRCS))
DEPS = $(patsubst $(SRCDIR)/%.c,$(BUILD)/%.d,$(SRCS))

# HEADERS to install
CPPFLAGS += -I$(INCDIR)
HEADERS = $(wildcard $(INCDIR)/*.h)

# library names
LIBRARY = $(BUILD)/libminiupnpc.a
ifneq (, $(findstring darwin, $(OS)))
  SHAREDLIBRARY = $(BUILD)/libminiupnpc.dylib
  SONAME = $(notdir $(basename $(SHAREDLIBRARY))).$(APIVERSION).dylib
  CPPFLAGS += -D_DARWIN_C_SOURCE
else
ifeq ($(JARSUFFIX), win32)
  SHAREDLIBRARY = $(BUILD)/miniupnpc.dll
else
  # Linux/BSD/etc.
  SHAREDLIBRARY = $(BUILD)/libminiupnpc.so
  SONAME = $(notdir $(SHAREDLIBRARY)).$(APIVERSION)
endif
endif

EXECUTABLES = $(addprefix $(BUILD)/, upnpc-static listdevices)
EXECUTABLES_ADDTESTS = $(addprefix $(BUILD)/, testminixml minixmlvalid \
    testupnpreplyparse testigddescparse testminiwget testportlistingparse)

TESTMINIXMLOBJS = $(addprefix $(BUILD)/, minixml.o igd_desc_parse.o testminixml.o)

TESTMINIWGETOBJS = $(addprefix $(BUILD)/, miniwget.o testminiwget.o connecthostport.o receivedata.o)

TESTUPNPREPLYPARSE = $(addprefix $(BUILD)/, testupnpreplyparse.o minixml.o upnpreplyparse.o)

TESTPORTLISTINGPARSE = $(addprefix $(BUILD)/, testportlistingparse.o minixml.o portlistingparse.o)

TESTADDR_IS_RESERVED = $(addprefix $(BUILD)/, testaddr_is_reserved.o addr_is_reserved.o)

TESTIGDDESCPARSE = $(addprefix $(BUILD)/, testigddescparse.o igd_desc_parse.o minixml.o \
                   miniupnpc.o miniwget.o upnpcommands.o upnpreplyparse.o \
                   minisoap.o connecthostport.o receivedata.o \
                   portlistingparse.o addr_is_reserved.o)

ifeq (, $(findstring amiga, $(OS)))
ifeq (, $(findstring mingw, $(OS))$(findstring cygwin, $(OS))$(findstring msys, $(OS)))
CFLAGS += -fPIC
endif
EXECUTABLES += $(BUILD)/upnpc-shared
TESTMINIWGETOBJS += $(BUILD)/minissdpc.o
TESTIGDDESCPARSE += $(BUILD)/minissdpc.o
LIBOBJS += $(BUILD)/minissdpc.o
endif

LIBDIR ?= lib
# install directories
ifeq ($(strip $(PREFIX)),)
INSTALLPREFIX ?= /usr
else
INSTALLPREFIX ?= $(PREFIX)
endif
INSTALLDIRINC = $(INSTALLPREFIX)/include/miniupnpc
INSTALLDIRLIB = $(INSTALLPREFIX)/$(LIBDIR)
INSTALLDIRBIN = $(INSTALLPREFIX)/bin
INSTALLDIRMAN = $(INSTALLPREFIX)/share/man
PKGCONFIGDIR = $(INSTALLDIRLIB)/pkgconfig

FILESTOINSTALL = $(LIBRARY) $(EXECUTABLES)
ifeq (, $(findstring amiga, $(OS)))
FILESTOINSTALL += $(SHAREDLIBRARY) $(BUILD)/miniupnpc.pc
endif


.PHONY:	install clean depend all check test everything \
	installpythonmodule updateversion

all:	$(LIBRARY) $(EXECUTABLES)

test:	check

check:	validateminixml validateminiwget validateupnpreplyparse \
	validateportlistingparse validateigddescparse validateaddr_is_reserved

everything:	all $(EXECUTABLES_ADDTESTS)

pythonmodule:	$(LIBRARY) $(SRCDIR)/miniupnpcmodule.c setup.py
	MAKE=$(MAKE) python setup.py build
	touch $@

installpythonmodule:	pythonmodule
	MAKE=$(MAKE) python setup.py install

pythonmodule3:	$(LIBRARY) $(SRCDIR)/miniupnpcmodule.c setup.py
	MAKE=$(MAKE) python3 setup.py build
	touch $@

installpythonmodule3:	pythonmodule3
	MAKE=$(MAKE) python3 setup.py install

validateminixml:	$(BUILD)/minixmlvalid
	@echo "minixml validation test"
	./$<
	touch $@

validateminiwget:	testminiwget.sh $(BUILD)/testminiwget $(BUILD)/minihttptestserver
	@echo "miniwget validation test"
	./$<
	touch $@

validateupnpreplyparse:	testupnpreplyparse.sh $(BUILD)/testupnpreplyparse
	@echo "upnpreplyparse validation test"
	./$<
	touch $@

validateportlistingparse:	$(BUILD)/testportlistingparse
	@echo "portlistingparse validation test"
	./$<
	touch $@

validateigddescparse:	$(BUILD)/testigddescparse
	@echo "igd desc parse validation test"
	./$< testdesc/new_LiveBox_desc.xml testdesc/new_LiveBox_desc.values
	./$< testdesc/linksys_WAG200G_desc.xml testdesc/linksys_WAG200G_desc.values
	touch $@

validateaddr_is_reserved:	$(BUILD)/testaddr_is_reserved
	@echo "addr_is_reserved() validation test"
	./$<
	touch $@

clean:
	$(RM) $(LIBRARY) $(SHAREDLIBRARY) $(EXECUTABLES) $(OBJS) $(BUILDINCLUDES)
	$(RM) $(EXECUTABLES_ADDTESTS)
	# clean python stuff
	$(RM) pythonmodule pythonmodule3
	$(RM) validateminixml validateminiwget validateupnpreplyparse
	$(RM) validateigddescparse
	$(RM) minihttptestserver
	$(RM) testaddr_is_reserved
	$(RM) -r build/ dist/
	#python setup.py clean
	# clean jnaerator stuff
	$(RM) _jnaerator.* java/miniupnpc_$(OS).jar

distclean: clean
	$(RM) $(JNAERATOR) java/*.jar java/*.class out.errors.txt

updateversion:	include/miniupnpc.h
	cp $< $<.bak
	sed 's/\(.*MINIUPNPC_API_VERSION\s\+\)[0-9]\+/\1$(APIVERSION)/' < $<.bak > $<

install:	updateversion $(FILESTOINSTALL)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(DESTDIR)$(INSTALLDIRLIB)
ifeq (, $(findstring amiga, $(OS)))
	$(INSTALL) -m 644 $(SHAREDLIBRARY) $(DESTDIR)$(INSTALLDIRLIB)/$(SONAME)
	ln -fs $(SONAME) $(DESTDIR)$(INSTALLDIRLIB)/$(notdir $(SHAREDLIBRARY))
	$(INSTALL) -d $(DESTDIR)$(PKGCONFIGDIR)
	$(INSTALL) -m 644 $(BUILD)/miniupnpc.pc $(DESTDIR)$(PKGCONFIGDIR)
endif
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRBIN)
ifneq (, $(findstring amiga, $(OS)))
	$(INSTALL) -m 755 $(BUILD)/upnpc-static $(DESTDIR)$(INSTALLDIRBIN)/upnpc
else
	$(INSTALL) -m 755 $(BUILD)/upnpc-shared $(DESTDIR)$(INSTALLDIRBIN)/upnpc
endif
	$(INSTALL) -m 755 external-ip.sh $(DESTDIR)$(INSTALLDIRBIN)/external-ip
ifeq (, $(findstring amiga, $(OS)))
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRMAN)/man3
	$(INSTALL) -m 644 man3/miniupnpc.3 $(DESTDIR)$(INSTALLDIRMAN)/man3/miniupnpc.3
ifneq (, $(findstring linux, $(OS)))
	gzip -f $(DESTDIR)$(INSTALLDIRMAN)/man3/miniupnpc.3
endif
endif

install-static:	updateversion $(FILESTOINSTALL)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(INSTALLDIRINC)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -m 644 $(LIBRARY) $(DESTDIR)$(INSTALLDIRLIB)
	$(INSTALL) -d $(DESTDIR)$(INSTALLDIRBIN)
	$(INSTALL) -m 755 external-ip.sh $(DESTDIR)$(INSTALLDIRBIN)/external-ip

cleaninstall:
	$(RM) -r $(DESTDIR)$(INSTALLDIRINC)
	$(RM) $(DESTDIR)$(INSTALLDIRLIB)/$(LIBRARY)
	$(RM) $(DESTDIR)$(INSTALLDIRLIB)/$(SHAREDLIBRARY)

$(BUILD)/miniupnpc.pc:	VERSION
	@$(MKDIR) $(@D)
	$(RM) $@
	echo "prefix=$(INSTALLPREFIX)" >> $@
	echo "exec_prefix=\$${prefix}" >> $@
	echo "libdir=\$${exec_prefix}/$(LIBDIR)" >> $@
	echo "includedir=\$${prefix}/include" >> $@
	echo "" >> $@
	echo "Name: miniUPnPc" >> $@
	echo "Description: UPnP IGD client lightweight library" >> $@
	echo "URL: https://miniupnp.tuxfamily.org/" >> $@
	echo "Version: $(VERSION)" >> $@
	echo "Libs: -L\$${libdir} -lminiupnpc" >> $@
	echo "Cflags: -I\$${includedir}" >> $@

depend:	$(DEPS)

$(LIBRARY):	$(LIBOBJS)
ifneq (, $(findstring darwin, $(OS)))
	$(LIBTOOL) -static -o $@ $?
else
	$(AR) crs $@ $?
endif

$(SHAREDLIBRARY):	$(LIBOBJS)
ifneq (, $(findstring darwin, $(OS)))
#	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(SONAME) -o $@ $^
	$(CC) -dynamiclib $(LDFLAGS) -Wl,-install_name,$(INSTALLDIRLIB)/$(SONAME) -o $@ $^
else
	$(CC) -shared $(LDFLAGS) -Wl,-soname,$(SONAME) -o $@ $^
endif

$(BUILD)/%.o:	$(SRCDIR)/%.c $(BUILD)/%.d
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(DEPS):	$(BUILDINCLUDES)

$(BUILD)/%.d:	$(SRCDIR)/%.c
	@$(MKDIR) $(@D)
	$(CC) $(CPPFLAGS) $(DEPFLAGS) -MT $@ -o $@ $<

$(BUILD)/upnpc-static:	$(BUILD)/upnpc.o $(LIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

$(BUILD)/upnpc-shared:	$(BUILD)/upnpc.o $(SHAREDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $^ $(LOADLIBES) $(LDLIBS)

$(BUILD)/listdevices:	$(BUILD)/listdevices.o $(LIBRARY)

$(BUILD)/testminixml:	$(TESTMINIXMLOBJS)

$(BUILD)/testminiwget:	$(TESTMINIWGETOBJS)

$(BUILD)/minixmlvalid:	$(addprefix $(BUILD)/, minixml.o minixmlvalid.o)

$(BUILD)/testupnpreplyparse:	$(TESTUPNPREPLYPARSE)

$(BUILD)/testigddescparse:	$(TESTIGDDESCPARSE)

$(BUILD)/testportlistingparse:	$(TESTPORTLISTINGPARSE)

$(BUILD)/testaddr_is_reserved:	$(TESTADDR_IS_RESERVED)

$(BUILD)/miniupnpcstrings.h:	miniupnpcstrings.h.in updateminiupnpcstrings.sh VERSION
	@$(MKDIR) $(@D)
	$(SH) updateminiupnpcstrings.sh $@ $<

# ftp tool supplied with OpenBSD can download files from http.
jnaerator-%.jar:
	wget $(JNAERATORBASEURL)/$@ || \
	curl -o $@ $(JNAERATORBASEURL)/$@ || \
	ftp $(JNAERATORBASEURL)/$@

jar: $(SHAREDLIBRARY)  $(JNAERATOR)
	$(JAVA) -jar $(JNAERATOR) $(JNAERATORARGS) \
	miniupnpc.h miniupnpc_declspec.h upnpcommands.h upnpreplyparse.h \
	igd_desc_parse.h miniwget.h upnperrors.h $(SHAREDLIBRARY) \
	-package fr.free.miniupnp -o . -jar java/miniupnpc_$(JARSUFFIX).jar -v

mvn_install:
	mvn install:install-file -Dfile=java/miniupnpc_$(JARSUFFIX).jar \
	 -DgroupId=com.github \
	 -DartifactId=miniupnp \
	 -Dversion=$(VERSION) \
	 -Dpackaging=jar \
	 -Dclassifier=$(JARSUFFIX) \
	 -DgeneratePom=true \
	 -DcreateChecksum=true

# make .deb packages
deb: /usr/share/pyshared/stdeb all
	(python setup.py --command-packages=stdeb.command bdist_deb)

# install .deb packages
ideb:
	(sudo dpkg -i deb_dist/*.deb)

/usr/share/pyshared/stdeb: /usr/share/doc/python-all-dev
	(sudo apt-get install python-stdeb)

/usr/share/doc/python-all-dev:
	(sudo apt-get install python-all-dev)

minihttptestserver:	minihttptestserver.o

print-%:
	@echo "$* = $($*)"

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif
