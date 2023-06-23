/*
	Copyright 2023 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include "types.h"
#include <cstring>

class HwRegister
{
public:
	HwRegister()
	{
		read8 = invalidRead<u8>;
		read16 = invalidRead<u16>;
		read32 = invalidRead<u32>;
		write8 = invalidWrite<u8>;
		write16 = invalidWrite<u16>;
		write32 = invalidWrite<u32>;
	}

	template<typename T>
	T read(u32 addr)
	{
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "invalid type size");
		switch (sizeof(T))
		{
		case 1:
			return (T)read8(addr);
		case 2:
			return (T)read16(addr);
		case 4:
			return (T)read32(addr);
		}
	}

	template<typename T>
	void write(u32 addr, T data)
	{
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "invalid type size");
		switch (sizeof(T))
		{
		case 1:
			write8(addr, data);
			break;
		case 2:
			write16(addr, data);
			break;
		case 4:
			write32(addr, data);
			break;
		}
	}

	template<typename T>
	void setReadHandler(T (*readHandler)(u32))
	{
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "invalid type size");
		switch (sizeof(T))
		{
		case 1:
			read8 = (u8 (*)(u32))readHandler;
			break;
		case 2:
			read16 = (u16 (*)(u32))readHandler;
			break;
		case 4:
			read32 = (u32 (*)(u32))readHandler;
			break;
		}
	}

	template<typename T>
	void setWriteHandler(void (*writeHandler)(u32, T))
	{
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4, "invalid type size");
		switch (sizeof(T))
		{
		case 1:
			write8 = (void (*)(u32, u8))writeHandler;
			break;
		case 2:
			write16 = (void (*)(u32, u16))writeHandler;
			break;
		case 4:
			write32 = (void (*)(u32, u32))writeHandler;
			break;
		}
	}

	template<typename T>
	static T invalidRead(u32 addr) {
		INFO_LOG(MEMORY, "Invalid register read<%d> %x", (int)sizeof(T), addr);
		return 0;
	}
	template<typename T>
	static void invalidWrite(u32 addr, T value) {
		INFO_LOG(MEMORY, "Invalid register write<%d> %x = %x", (int)sizeof(T), addr, (int)value);
	}

private:
	u8 (*read8)(u32 addr);
	void (*write8)(u32 addr, u8 value);

	u16 (*read16)(u32 addr);
	void (*write16)(u32 addr, u16 value);

	u32 (*read32)(u32 addr);
	void (*write32)(u32 addr, u32 value);
};

template<u32 *Module, u32 AddressMask = 0xff, u32 BaseAddress = 0>
class MMRegister : public HwRegister
{
public:
	template<u32 Addr, typename T = u32, u32 Mask = 0xffffffff, u32 OrMask = 0>
	void setReadWrite()
	{
		setReadHandler(readModule<Addr, T>);
		setWriteHandler(writeModule<Addr, T, Mask, OrMask>);
	}

	template<u32 Addr, typename T = u32>
	void setReadOnly()
	{
		setReadHandler(readModule<Addr, T>);
	}

	template<u32 Addr, typename T = u32, u32 Mask = 0xffffffff, u32 OrMask = 0>
	void setWriteOnly()
	{
		setWriteHandler(writeModule<Addr, T, Mask, OrMask>);
	}

private:
	template<u32 Addr, typename T = u32>
	static T readModule(u32 addr)
	{
		return (T)Module[((Addr - BaseAddress) & AddressMask) / 4];
	}

	template<u32 Addr, typename T = u32, u32 Mask = 0xffffffff, u32 OrMask = 0>
	static void writeModule(u32 addr, T data)
	{
		Module[((Addr - BaseAddress) & AddressMask) / 4] = (u32)((data & Mask) | OrMask);
	}
};

template<u32 *Module, size_t Size, u32 AddressMask = 0xff, u32 BaseAddress = 0>
class RegisterBank
{
	using RegisterType = MMRegister<Module, AddressMask, BaseAddress>;

	static_assert((((Size - 1) * sizeof(u32)) & AddressMask) == ((Size - 1) * sizeof(u32)), "Size too big for address mask");
	RegisterType registers[Size];

public:
	void init()
	{
		for (RegisterType& reg : registers)
			reg = {};
	}

	void reset()
	{
		memset(Module, 0, Size * sizeof(u32));
	}

	void term()
	{
	}

	template<u32 Addr>
	RegisterType& getRegister()
	{
		constexpr size_t index = ((Addr - BaseAddress) & AddressMask) / sizeof(u32);
		static_assert(index < Size, "Out of bound register");
		return registers[index];
	}

	// Configure the register at the given address to be readable and writable, with optional write masks.
	// Only accesses of the specified type size (defaults to u32) are allowed.
	template<u32 Addr, typename T = u32, u32 Mask = 0xffffffff, u32 OrMask = 0>
	void setRW()
	{
		getRegister<Addr>().template setReadWrite<Addr, T, Mask, OrMask>();
	}

	// Configure the register at the given address to use the given read and write handlers
	// Only accesses of the handlers type size are allowed.
	template<u32 Addr, typename T>
	void setHandlers(T (*readHandler)(u32), void (*writeHandler)(u32, T))
	{
		RegisterType& reg = getRegister<Addr>();
		reg.setReadHandler(readHandler);
		reg.setWriteHandler(writeHandler);
	}

	// Configure the register at the given address to use the given write handler and be readable
	// Only accesses of the specified or inferred type size (defaults to u32) are allowed.
	template<u32 Addr, typename T>
	void setWriteHandler(void (*writeHandler)(u32, T))
	{
		RegisterType& reg = getRegister<Addr>();
		reg.template setReadOnly<Addr, T>();
		reg.setWriteHandler(writeHandler);
	}

	// Configure the register at the given address to be write only, with optional write handler or write masks.
	// Write masks are ignored if a write handler is specified.
	template<u32 Addr, typename T = u32, u32 Mask = 0xffffffff, u32 OrMask = 0>
	void setWriteOnly(void (*writeHandler)(u32, T) = nullptr)
	{
		RegisterType& reg = getRegister<Addr>();
		if (writeHandler != nullptr)
			reg.setWriteHandler(writeHandler);
		else
			reg.template setWriteOnly<Addr, T, Mask, OrMask>();
	}

	// Configure the register at the given address to be read only, with optional read handler
	template<u32 Addr, typename T = u32>
	void setReadOnly(T (*readHandler)(u32) = nullptr)
	{
		RegisterType& reg = getRegister<Addr>();
		if (readHandler != nullptr)
			reg.setReadHandler(readHandler);
		else
			reg.template setReadOnly<Addr, T>();
	}

	size_t getRegIndex(u32 addr)
	{
		return ((addr - BaseAddress) & AddressMask) / sizeof(u32);
	}

	// Read handler for the bank
	template<typename T>
	T read(u32 addr)
	{
		size_t index = getRegIndex(addr);
		if (index >= Size)
		{
			INFO_LOG(MEMORY, "Out of bound read @ %x", addr);
			return 0;
		}
		if (addr & 3)
		{
			INFO_LOG(MEMORY, "Unaligned register read @ %x", addr);
			return 0;
		}
		return registers[index].template read<T>(addr);
	}

	// Write handler for the bank
	template<typename T>
	void write(u32 addr, T data)
	{
		size_t index = getRegIndex(addr);
		if (index >= Size)
			INFO_LOG(MEMORY, "Out of bound write @ %x = %x", addr, (int)data);
		else if (addr & 3)
			INFO_LOG(MEMORY, "Unaligned register write @ %x = %x", addr, (int)data);
		else
			registers[index].write(addr, data);
	}
};

template<typename T>
T ReadMemArr(const u8 *array, u32 addr)
{
	return *(const T *)&array[addr];
}

template<typename T>
void WriteMemArr(u8 *array, u32 addr, T data)
{
	*(T *)&array[addr] = data;
}
