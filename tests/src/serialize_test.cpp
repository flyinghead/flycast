#include "gtest/gtest.h"
#include "types.h"
#include "hw/mem/_vmem.h"
#include "hw/maple/maple_cfg.h"
#include "hw/maple/maple_devs.h"
#include "emulator.h"

class SerializeTest : public ::testing::Test {
protected:
	void SetUp() override {
		if (!_vmem_reserve())
			die("_vmem_reserve failed");
		dc_init();
		dc_reset(true);
	}
};

TEST_F(SerializeTest, SizeTest)
{
	settings.input.maple_devices[0] = MDT_SegaController;
	settings.input.maple_expansion_devices[0][0] = MDT_SegaVMU;
	settings.input.maple_expansion_devices[0][1] = MDT_SegaVMU;
	settings.input.maple_devices[1] = MDT_SegaController;
	settings.input.maple_expansion_devices[1][0] = MDT_SegaVMU;
	settings.input.maple_expansion_devices[1][1] = MDT_SegaVMU;
	mcfg_CreateDevices();

	unsigned int total_size = 0;
	void *data = nullptr;
	ASSERT_TRUE(dc_serialize(&data, &total_size));
	ASSERT_EQ(28145464u, total_size);
}



