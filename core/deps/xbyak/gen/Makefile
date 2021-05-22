TARGET=../xbyak/xbyak_mnemonic.h
BIN=sortline gen_code gen_avx512
CFLAGS=-I../ -O2 -DXBYAK_NO_OP_NAMES -Wall -Wextra -Wno-missing-field-initializers
all: $(TARGET)
sortline: sortline.cpp
	$(CXX) $(CFLAGS) $< -o $@
gen_code: gen_code.cpp ../xbyak/xbyak.h avx_type.hpp
	$(CXX) $(CFLAGS) $< -o $@
gen_avx512: gen_avx512.cpp ../xbyak/xbyak.h avx_type.hpp
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

clean:
	$(RM) $(BIN) $(TARGET)
