/*
	Copyright 2026 flyinghead

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
#include <deque>

namespace modem
{

class InStream
{
public:
	virtual ~InStream() = default;
	virtual int read() = 0;
	virtual int available() = 0;
};

class OutStream
{
public:
	virtual ~OutStream() = default;
	virtual void write(u8 v) = 0;
};

class Transformer : public InStream, public OutStream
{
};

class PipeOut : public OutStream
{
public:
	PipeOut(Transformer& transformer, OutStream& out)
		: transformer(transformer), out(out)
	{}

	void write(u8 v) override
	{
		transformer.write(v);
		while (transformer.available() != 0)
			out.write(transformer.read());
	}

private:
	Transformer& transformer;
	OutStream& out;
};

class PipeIn : public InStream
{
public:
	PipeIn(Transformer& transformer, InStream& in)
		: transformer(transformer), in(in)
	{}

	int read() override {
		flushIn();
		return transformer.read();
	}
	int available() override {
		flushIn();
		return transformer.available();
	}

private:
	void flushIn() {
		while (in.available() != 0)
			transformer.write(in.read());
	}

	Transformer& transformer;
	InStream& in;
};

class BufferedTransformer : public Transformer
{
public:
	int read() override
	{
		if (buffer.empty())
			return -1;
		int c = buffer.front();
		buffer.pop_front();
		return c;
	}

	int available() override {
		return buffer.size();
	}

	virtual void reset() {
		buffer.clear();
	}

protected:
	std::deque<u8> buffer;
};

}
