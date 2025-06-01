#include "gtest/gtest.h"

#include "input/gamepad_device.h"
#include "input/mapping.h"

class TestGamepadDeviceStub : public GamepadDevice
{
public:
    TestGamepadDeviceStub() : GamepadDevice(0, "TestGamepadDeviceStub")
    {
        set_maple_port(0);
        loadMapping();
    }

    void resetMappingToDefault(bool arcade, bool gamepad) override
    {
        input_mapper = getDefaultMapping();
    }

    std::shared_ptr<InputMapping> getDefaultMapping() override
    {
        std::shared_ptr<InputMapping> inputMapping = std::make_shared<InputMapping>();

        const DreamcastKey mappedKey1 = DreamcastKey::DC_BTN_C;
        const InputMapping::ButtonCombo combo1{
            InputMapping::InputSet{
                InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG},
                InputMapping::InputDef{100, InputMapping::InputDef::InputType::BUTTON},
                InputMapping::InputDef{3, InputMapping::InputDef::InputType::AXIS_POS}
            },
            true
        };
        inputMapping->set_button(mappedKey1, combo1);
        const DreamcastKey mappedKey2 = DreamcastKey::DC_BTN_D;
        const InputMapping::ButtonCombo combo2{
            InputMapping::InputSet{
                InputMapping::InputDef{200, InputMapping::InputDef::InputType::BUTTON},
                InputMapping::InputDef{789, InputMapping::InputDef::InputType::AXIS_POS},
                InputMapping::InputDef{3, InputMapping::InputDef::InputType::AXIS_POS}
            },
            false
        };
        inputMapping->set_button(mappedKey2, combo2);
        const DreamcastKey mappedKey3 = DreamcastKey::DC_BTN_Z;
        const InputMapping::ButtonCombo combo3{
            InputMapping::InputSet{
                InputMapping::InputDef{3, InputMapping::InputDef::InputType::AXIS_POS},
                InputMapping::InputDef{100, InputMapping::InputDef::InputType::BUTTON},
                InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG}
            },
            true
        };
        inputMapping->set_button(mappedKey3, combo3);
        inputMapping->set_button(DreamcastKey::DC_BTN_A, 100);
        inputMapping->set_button(DreamcastKey::DC_BTN_B, 200);
        inputMapping->set_axis(DreamcastKey::DC_AXIS_LT, 1, true);
        inputMapping->set_axis(DreamcastKey::DC_AXIS_RT, 2, true);
        inputMapping->set_axis(DreamcastKey::DC_AXIS_UP, 3, true);
        inputMapping->set_axis(DreamcastKey::DC_AXIS_DOWN, 3, false);
        inputMapping->name = "save test case";
        inputMapping->dead_zone = 0.33;
        inputMapping->rumblePower = 98;
        inputMapping->saturation = 0.50;

        return inputMapping;
    }
};

class GamepadeInputHandlingTest : public ::testing::Test
{
protected:
    TestGamepadDeviceStub gamepadDevice;

    void SetUp() override
    {
        kcode[0] = ~0;
    }

    void TearDown() override
    {
        kcode[0] = ~0;
    }
};

TEST_F(GamepadeInputHandlingTest, combo_test)
{
    gamepadDevice.gamepad_btn_input(100, true);
    u32 currentCode = ~0;
    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_A);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_axis_input(3, 30000);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_axis_input(191, -30000);
    ASSERT_EQ(kcode[0], currentCode); // combo 1 and 3 should not be active - wrong order
    gamepadDevice.gamepad_axis_input(191, 0);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_btn_input(100, false);
    currentCode |= static_cast<u32>(DreamcastKey::DC_BTN_A);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_btn_input(100, true);
    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_A);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_axis_input(191, -30000); // combo 3 should now be active
    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_Z);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_axis_input(789, 30000);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_axis_input(191, 0);
    currentCode |= static_cast<u32>(DreamcastKey::DC_BTN_Z);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_btn_input(100, false);
    currentCode |= static_cast<u32>(DreamcastKey::DC_BTN_A);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_btn_input(200, true); // combo 2 will take precedence over B
    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_D);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_axis_input(789, 0);
    currentCode |= static_cast<u32>(DreamcastKey::DC_BTN_D);
    ASSERT_EQ(kcode[0], currentCode);
    gamepadDevice.gamepad_btn_input(200, false);
    gamepadDevice.gamepad_btn_input(200, true); // This activates B now
    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_B);
    ASSERT_EQ(kcode[0], currentCode);
}

TEST_F(GamepadeInputHandlingTest, combo_multi_release)
{
    u32 currentCode = ~0;
    gamepadDevice.gamepad_axis_input(191, -30000);
    gamepadDevice.gamepad_btn_input(100, true); // Activates A
    gamepadDevice.gamepad_axis_input(3, 30000); // Activates C (combo of above 3)

    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_A);
    currentCode &= ~static_cast<u32>(DreamcastKey::DC_BTN_C);
    ASSERT_EQ(kcode[0], currentCode);

    gamepadDevice.gamepad_btn_input(100, false); // Should deactivate both A and C

    currentCode |= static_cast<u32>(DreamcastKey::DC_BTN_A);
    currentCode |= static_cast<u32>(DreamcastKey::DC_BTN_C);
    ASSERT_EQ(kcode[0], currentCode);
}
