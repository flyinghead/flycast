#include "gtest/gtest.h"
#include "hw/modem/v42.h"

namespace modem
{

class TestNetOut : public OutStream
{
public:
	void write(u8 v) override {
		buffer.push_back(v);
	}

	std::vector<u8> buffer;
};

class TestNetIn : public InStream
{
public:
	TestNetIn(const std::vector<u8>& data = {})
		: buffer(data)
	{}
	int read() override
	{
		if (buffer.empty()) {
			return -1;
		}
		else
		{
			u8 v = buffer.front();
			buffer.erase(buffer.begin());
			return v;
		}
	}
	int available() override {
		return buffer.size();
	}

	std::vector<u8> buffer;
};

class V42Test : public ::testing::Test
{
protected:
	void setConnected(V42Protocol& v42) {
		v42.phase = V42Protocol::Connected;
	}
	void sendFrame(V42Protocol& v42, const std::vector<u8>& data) {
		v42.sendFrame(data);
	}
	void sendFlag(V42Protocol& v42) {
		v42.sendFlag();
	}

	TestNetOut netOut;
	TestNetIn netIn;
};

TEST_F(V42Test, bitStuffing)
{
	// nothing to escape
	V42Protocol v42 { netIn, netOut };
	setConnected(v42);
	sendFrame(v42, { 0x0f, 0x0f, 0x0f });
	ASSERT_EQ(0x7e, v42.read());
	ASSERT_EQ(0x0f, v42.read());
	ASSERT_EQ(0x0f, v42.read());
	ASSERT_EQ(0x0f, v42.read());

	// flags only
	v42.reset();
	setConnected(v42);
	sendFlag(v42);
	sendFlag(v42);
	ASSERT_EQ(0x7e, v42.read());
	ASSERT_EQ(0x7e, v42.read());

	v42.reset();
	setConnected(v42);
	sendFrame(v42, { 0xff, 0xff });
	ASSERT_EQ(0x7e, v42.read());
	ASSERT_EQ(0xdf, v42.read());
	ASSERT_EQ(0xf7, v42.read());
	ASSERT_TRUE((v42.read() & 7) == 5);

	v42.reset();
	setConnected(v42);
	sendFrame(v42, { 0xff, 0xff, 0xff });
	ASSERT_EQ(0x7e, v42.read());
	ASSERT_EQ(0xdf, v42.read());
	ASSERT_EQ(0xf7, v42.read());
	ASSERT_EQ(0x7d, v42.read());
	ASSERT_TRUE((v42.read() & 3) == 3);

	v42.reset();
	setConnected(v42);
	sendFrame(v42, { 0xdf, 0xf7, 0x7d });
	ASSERT_EQ(0x7e, v42.read());
	ASSERT_EQ(0x9f, v42.read());
	ASSERT_EQ(0xcf, v42.read());
	ASSERT_EQ(0xe7, v42.read());
	ASSERT_TRUE((v42.read() & 0xf) == 3);

	v42.reset();
	setConnected(v42);
	sendFrame(v42, { 0xf8, 0x01, 0x00 });
	ASSERT_EQ(0x7e, v42.read());
	ASSERT_EQ(0xf8, v42.read());
	ASSERT_EQ(0x02, v42.read());
	ASSERT_EQ(0x00, v42.read());
}

TEST_F(V42Test, txMultipleFrames)
{
	const auto& unstuff = [this](V42Protocol& v42, unsigned count, u32& bitBuffer, int& bitBufSize, int& ones) -> std::vector<u8>
	{
		std::vector<u8> out;
		while (out.size() < count)
		{
			u8 b = v42.read();
			int i = 0;
			if (ones == 5)
			{
				ones = 0;
				if ((b & 1) == 0)
				{
					// not a flag
					i = 1;
					b >>= 1;
				}
			}
			for (; i < 8; i++)
			{
				bitBuffer |= (b & 1) << bitBufSize++;
				if (b & 1)
				{
					if (++ones == 5 && i < 7)
					{
						if (b & 2) {
							// flag
							ones = 0;
						}
						else
						{
							i++; // skip stuffed bit
							b >>= 1;
							ones = 0;
						}
					}
				}
				else {
					ones = 0;
				}
				b >>= 1;
			}
			while (bitBufSize >= 8)
			{
				out.push_back(bitBuffer & 0xff);
				bitBuffer >>= 8;
				bitBufSize -= 8;
			}
		}
		return out;
	};

	srand(42);
	V42Protocol v42 { netIn, netOut };
	setConnected(v42);
	u32 bitBuf = 0;
	int bitBufSize = 0;
	int ones = 0;
	for (int j = 0; j < 10; j++)
	{
		std::vector<u8> data;
		for (int i = 0; i < 1024; i++)
			data.push_back(rand() & 0xff);
		sendFrame(v42, data);

		std::vector<u8> out = unstuff(v42, 1, bitBuf, bitBufSize, ones);
		ASSERT_EQ(0x7e, out[0]);
		out = unstuff(v42, 1024, bitBuf, bitBufSize, ones);
		while (out[0] == 0x7e)
		{
			auto tmp = unstuff(v42, 1, bitBuf, bitBufSize, ones);
			out.erase(out.begin());
			out.push_back(tmp[0]);
		}
		out = unstuff(v42, 3, bitBuf, bitBufSize, ones);
		ASSERT_EQ(0x7e, out[2]);
	}
}

} // namespace modem
