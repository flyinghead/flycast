#include "gtest/gtest.h"

#include "input/mapping.h"

class MultiBindMappingTest : public ::testing::Test
{
protected:
    InputMapping inputMapping;

    void SetUp() override
    {}
};

TEST_F(MultiBindMappingTest, get_button_code__found)
{
    // Arrange
    const DreamcastKey mappedKey = DreamcastKey::DC_BTN_A;
    const u32 mappedCode = 4294967294;
    inputMapping.set_button(mappedKey, mappedCode);

    // Act
    const u32 retrievedCode = inputMapping.get_button_code(0, mappedKey);

    // Assert
    ASSERT_EQ(retrievedCode, mappedCode);
}

TEST_F(MultiBindMappingTest, get_button_code__not_found)
{
    // Arrange
    inputMapping.set_button(DreamcastKey::DC_BTN_A, 4294967294);

    // Act
    const u32 retrievedCode = inputMapping.get_button_code(0, DreamcastKey::DC_BTN_B);

    // Assert
    const u32 expectedCode = InputMapping::InputDef::INVALID_CODE;
    ASSERT_EQ(retrievedCode, expectedCode);
}

TEST_F(MultiBindMappingTest, get_button_combo__found)
{
    // Arrange
    const DreamcastKey mappedKey = DreamcastKey::DC_BTN_A;
    const InputMapping::ButtonCombo combo{
        InputMapping::InputSet{
            InputMapping::InputDef{123, InputMapping::InputDef::InputType::BUTTON},
            InputMapping::InputDef{987, InputMapping::InputDef::InputType::AXIS_POS},
            InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG}
        },
        true
    };
    inputMapping.set_button(mappedKey, combo);

    // Act
    const InputMapping::ButtonCombo retrievedCombo = inputMapping.get_button_combo(0, mappedKey);

    // Assert
    ASSERT_EQ(retrievedCombo, combo);
}

TEST_F(MultiBindMappingTest, get_button_combo__not_found)
{
    // Arrange
    inputMapping.set_button(DreamcastKey::DC_BTN_A, 4294967294);

    // Act
    const InputMapping::ButtonCombo retrievedCombo = inputMapping.get_button_combo(0, DreamcastKey::DC_BTN_B);

    // Assert
    EXPECT_EQ(retrievedCombo, InputMapping::ButtonCombo());
    EXPECT_TRUE(retrievedCombo.inputs.empty());
}

TEST_F(MultiBindMappingTest, get_button_combo__not_found__overlapping_element)
{
    // Arrange
    const DreamcastKey mappedKey1 = DreamcastKey::DC_BTN_A;
    const InputMapping::ButtonCombo combo1{
        InputMapping::InputSet{
            InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG},
            InputMapping::InputDef{123, InputMapping::InputDef::InputType::BUTTON},
            InputMapping::InputDef{987, InputMapping::InputDef::InputType::AXIS_POS}
        },
        true
    };
    inputMapping.set_button(mappedKey1, combo1);
    const DreamcastKey mappedKey2 = DreamcastKey::DC_BTN_B;
    // Contains all of the same keys and is not sequential, so the above should be removed when this is added
    const InputMapping::ButtonCombo combo2{
        InputMapping::InputSet{
            InputMapping::InputDef{123, InputMapping::InputDef::InputType::BUTTON},
            InputMapping::InputDef{987, InputMapping::InputDef::InputType::AXIS_POS},
            InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG}
        },
        false
    };
    inputMapping.set_button(mappedKey2, combo2);

    // Act
    const InputMapping::ButtonCombo retrievedCombo = inputMapping.get_button_combo(0, mappedKey1);

    // Assert
    EXPECT_EQ(retrievedCombo, InputMapping::ButtonCombo());
}

TEST_F(MultiBindMappingTest, get_button_combo__found__sequential_not_overlapped)
{
    // Arrange
    const DreamcastKey mappedKey1 = DreamcastKey::DC_BTN_A;
    const InputMapping::ButtonCombo combo1{
        InputMapping::InputSet{
            InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG},
            InputMapping::InputDef{123, InputMapping::InputDef::InputType::BUTTON},
            InputMapping::InputDef{987, InputMapping::InputDef::InputType::AXIS_POS}
        },
        true
    };
    inputMapping.set_button(mappedKey1, combo1);
    const DreamcastKey mappedKey2 = DreamcastKey::DC_BTN_B;
    const InputMapping::ButtonCombo combo2{
        InputMapping::InputSet{
            InputMapping::InputDef{123, InputMapping::InputDef::InputType::BUTTON},
            InputMapping::InputDef{987, InputMapping::InputDef::InputType::AXIS_POS},
            InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG}
        },
        true
    };
    inputMapping.set_button(mappedKey2, combo2);
    const DreamcastKey mappedKey3 = DreamcastKey::DC_BTN_C;
    const InputMapping::ButtonCombo combo3{
        InputMapping::InputSet{
            InputMapping::InputDef{987, InputMapping::InputDef::InputType::AXIS_POS},
            InputMapping::InputDef{123, InputMapping::InputDef::InputType::BUTTON},
            InputMapping::InputDef{191, InputMapping::InputDef::InputType::AXIS_NEG}
        },
        true
    };
    inputMapping.set_button(mappedKey3, combo3);

    // Act
    const InputMapping::ButtonCombo retrievedCombo1 = inputMapping.get_button_combo(0, mappedKey1);
    const InputMapping::ButtonCombo retrievedCombo2 = inputMapping.get_button_combo(0, mappedKey2);
    const InputMapping::ButtonCombo retrievedCombo3 = inputMapping.get_button_combo(0, mappedKey3);

    // Assert
    EXPECT_EQ(retrievedCombo1, combo1);
    EXPECT_EQ(retrievedCombo2, combo2);
    EXPECT_EQ(retrievedCombo3, combo3);
}
