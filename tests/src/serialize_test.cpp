#include "gtest/gtest.h"
#include "types.h"
#include "hw/mem/addrspace.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "emulator.h"
#include "cfg/option.h"

class SerializeTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		if (!addrspace::reserve())
			die("addrspace::reserve failed");
		emu.init();
		emu.dc_reset(true);
	}
};

TEST_F(SerializeTest, SizeTest)
{
	using namespace config;
	Settings::instance().reset();
	MapleMainDevices[0] = MDT_SegaController;
	MapleExpansionDevices[0][0] = MDT_SegaVMU;
	MapleExpansionDevices[0][1] = MDT_SegaVMU;
	MapleMainDevices[1] = MDT_SegaController;
	MapleExpansionDevices[1][0] = MDT_SegaVMU;
	MapleExpansionDevices[1][1] = MDT_SegaVMU;
	mcfg_CreateDevices();

	std::vector<char> data(30000000);
	Serializer ser(data.data(), data.size());
	dc_serialize(ser);
	ASSERT_EQ(28050889u, ser.size());
}

TEST(SerializerBufferTest, BufferOverflowThrowsException)
{
	std::vector<char> smallBuffer(10);
	Serializer ser(smallBuffer.data(), smallBuffer.size());
	std::vector<int> largeData(100);
	ASSERT_THROW(ser.serialize(largeData.data(), largeData.size()), Serializer::Exception);
}

TEST(SerializerBufferTest, BufferWithinLimitSucceeds)
{
	std::vector<char> buffer(100);
	Serializer ser(buffer.data(), buffer.size());

	// Record the header size (which includes e.g. version)
	size_t initialSize = ser.size();

	std::vector<int> data(10);
	ASSERT_NO_THROW(ser.serialize(data.data(), data.size()));
	ASSERT_EQ(ser.size(), initialSize + sizeof(int) * 10);
}

TEST(SerializerBufferTest, ExactBufferSizeSucceeds)
{
	// Determine the header size (which includes e.g. version)
	Serializer dryRun(nullptr, 1000);
	size_t headerSize = dryRun.size();

	// Serialize data to buffer of equal size
	std::vector<char> buffer(headerSize + sizeof(int) * 10);
	std::vector<int> data(10);
	Serializer ser(buffer.data(), buffer.size());
	ASSERT_NO_THROW(ser.serialize(data.data(), data.size()));
	ASSERT_EQ(ser.size(), buffer.size());
}

TEST(SerializerBufferTest, OffByOneOverflowThrowsException)
{
	// Create a buffer that's 1 byte too small
	std::vector<char> buffer(sizeof(int) * 10 - 1);
	std::vector<int> data(10);
	Serializer ser(buffer.data(), buffer.size());
	ASSERT_THROW(ser.serialize(data.data(), data.size()), Serializer::Exception);
}
