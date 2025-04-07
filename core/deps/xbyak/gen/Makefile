TARGET=../xbyak/xbyak_mnemonic.h
BIN=sortline gen_code gen_avx512
CFLAGS=-I../ -I ./ -Wall -Wextra -Wno-missing-field-initializers $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)
all: $(TARGET) ../CMakeLists.txt ../meson.build ../readme.md ../readme.txt

avx_type_def.h: ../xbyak/xbyak.h
	sed -n '/@@@begin of avx_type_def.h/,/@@@end of avx_type_def.h/p' $< > $@

avx_type.hpp: avx_type_def.h
sortline: sortline.cpp
	$(CXX) $(CFLAGS) $< -o $@
gen_code: gen_code.cpp avx_type.hpp
	$(CXX) $(CFLAGS) $< -o $@
gen_avx512: gen_avx512.cpp avx_type.hpp
	$(CXX) $(CFLAGS) $< -o $@

$(TARGET): $(BIN)
	./gen_code | ./sortline > $@
	echo "#ifdef XBYAK_ENABLE_OMITTED_OPERAND" >> $@
	./gen_code omit | ./sortline >> $@
	echo "#endif" >>$@
	./gen_code fixed >> $@
	echo "#ifndef XBYAK_DISABLE_AVX512" >> $@
	./gen_avx512 | ./sortline >> $@
	echo "#ifdef XBYAK64" >> $@
	./gen_avx512 64 | ./sortline >> $@
	echo "#endif" >> $@
	echo "#endif" >> $@

VER=$(shell head -n 1 ../xbyak/xbyak_mnemonic.h|grep -o "[0-9.]*")
../CMakeLists.txt: $(TARGET)
	sed -i -e 's/CXX VERSION [0-9.]*/CXX VERSION $(VER)/' $@

../meson.build: $(TARGET)
	sed -i -e "s/version: '[0-9.]*',/version: '$(VER)',/" $@

../readme.md: $(TARGET)
	sed -l 2 -i -e "s/# Xbyak [0-9.]*/# Xbyak $(VER)/" $@

../readme.txt: $(TARGET)
	sed -l 2 -i -e "s/Xbyak [0-9.]*/Xbyak $(VER)/" $@

clean:
	$(RM) $(BIN) $(TARGET) avx_type_def.h
