#include "cfg/option.h"
#include "emulator.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "hw/mem/addrspace.h"
#include "types.h"
#include "gtest/gtest.h"

class SerializeTest : public ::testing::Test
{
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
	ASSERT_EQ(28050642u, ser.size());
}
