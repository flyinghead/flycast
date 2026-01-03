#include "gtest/gtest.h"
#include "hw/modem/v42bis.h"
#include <stdlib.h>

#define LOREM1 "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Pellentesque sed faucibus enim. Proin quis turpis augue. " \
			"Donec nec ornare odio, eu cursus nulla. Cras quis iaculis odio. Vestibulum in lacus vel sapien feugiat congue a vitae tortor. " \
			"Curabitur fringilla dolor et ex condimentum, et elementum neque iaculis. Proin gravida facilisis facilisis. "

namespace modem::v42b
{

class V42bisTest : public ::testing::Test
{
protected:
	std::string compDecomp(const std::string& in, Compressor& comp, Decompressor& decomp)
	{
		for (char c : in)
			comp.write(c);
		comp.flush();

		while (true)
		{
			int c = comp.read();
			if (c == -1)
				break;
			decomp.write(c);
		}
		std::string out;
		while (true)
		{
			int c = decomp.read();
			if (c == -1)
				break;
			out += (char)c;
		}
		return out;
	}
};

TEST_F(V42bisTest, transparentMode)
{
	Compressor comp;
	Decompressor decomp;
	std::string in = "Lorem ipsum dolor sit amet";
	std::string out = compDecomp(in, comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, escapeCharacter)
{
	constexpr int EID = 1;
	Compressor comp;
	comp.write('\0');
	ASSERT_EQ('\0', comp.read());
	ASSERT_EQ(EID, comp.read());
	ASSERT_EQ(-1, comp.read());

	comp.write('\0');
	ASSERT_EQ('\0', comp.read());
	// no longer escaped
	ASSERT_EQ(-1, comp.read());

	comp.write('3');
	ASSERT_EQ('3', comp.read());
	ASSERT_EQ(EID, comp.read());
	ASSERT_EQ(-1, comp.read());

	Decompressor decomp;
	decomp.write('\0');
	decomp.write(EID);
	ASSERT_EQ('\0', decomp.read());
	ASSERT_EQ(-1, decomp.read());

	decomp.write('\0');
	ASSERT_EQ('\0', decomp.read());
	ASSERT_EQ(-1, decomp.read());

	decomp.write('3');
	decomp.write(EID);
	ASSERT_EQ('3', decomp.read());
	ASSERT_EQ(-1, decomp.read());
}

TEST_F(V42bisTest, compressMode)
{
	Compressor comp;
	comp.changeMode();
	Decompressor decomp;
	std::string in = "Lorem ipsum dolor sit amet";
	std::string out = compDecomp(in, comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, compressMode2)
{
	Compressor comp(1024);
	comp.changeMode();
	Decompressor decomp(1024);
	std::string in = LOREM1
			"Maecenas aliquam commodo velit, non porta neque varius non. Donec accumsan, ipsum vitae porttitor porta, "
			"elit ipsum efficitur nulla, at commodo elit lacus nec mauris. Etiam a interdum metus. Proin auctor dolor a semper luctus. "
			"Aliquam sagittis orci non elit ornare gravida. Phasellus eros eros, bibendum eget dolor at, ultrices consequat libero. "
			"Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Etiam eros eros, "
			"convallis sit amet lectus a, rutrum maximus odio. Vivamus dolor nunc, sodales sed nisl vitae, rhoncus commodo lectus.";
	std::string out = compDecomp(in, comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, compressRandom)
{
	Compressor comp(4096);
	comp.changeMode();
	Decompressor decomp(4096);
	std::string in;
	srand(42);
	for (int i = 0; i < 8192; i++)
		in.push_back(rand() & 0xff);
	std::string out = compDecomp(in, comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, switchToCompressed)
{
	std::string in = LOREM1;
	Compressor comp;
	Decompressor decomp;
	size_t i = 0;
	for (; i < in.length() / 2; i++)
		comp.write(in[i]);
	comp.changeMode();
	std::string out = compDecomp(in.substr(i), comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, switchToTransparent)
{
	std::string in = LOREM1;
	Compressor comp;
	Decompressor decomp;
	comp.changeMode();
	size_t i = 0;
	for (; i < in.length() / 2; i++)
		comp.write(in[i]);
	comp.changeMode();
	std::string out = compDecomp(in.substr(i), comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, appendix_II_4_3)
{
	std::string in = "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
	Compressor comp;
	comp.changeMode();
	Decompressor decomp;
	std::string out = compDecomp(in, comp, decomp);
	ASSERT_EQ(out, in);
}

TEST_F(V42bisTest, flush)
{
	std::string in = "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc" LOREM1;
	for (int j = 1; j < 10; j++)
	{
		Compressor comp;
		comp.changeMode();
		Decompressor decomp;
		for (size_t i = 0; i < in.size(); i += j)
		{
			for (int k = 0; k < j; k++)
			{
				if (i + k == in.size())
					break;
				comp.write(in[i + k]);
			}
			comp.flush();
		}
		std::string out = compDecomp("", comp, decomp);
		ASSERT_EQ(out, in);
	}
}

TEST_F(V42bisTest, changeMode)
{
	std::string in = "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc" LOREM1;
	for (int j = 1; j < 10; j++)
	{
		Compressor comp;
		comp.changeMode();
		Decompressor decomp;
		for (size_t i = 0; i < in.size(); i += j)
		{
			for (int k = 0; k < j; k++)
			{
				if (i + k == in.size())
					break;
				comp.write(in[i + k]);
			}
			comp.changeMode();
		}
		std::string out = compDecomp("", comp, decomp);
		ASSERT_EQ(out, in);
	}
}


TEST_F(V42bisTest, veryLongRepeatingString)
{
	std::string in(4096, 'z');
	Compressor comp;
	Decompressor decomp;
	std::string out = compDecomp(in, comp, decomp);
	ASSERT_EQ(out, in);
}

}

