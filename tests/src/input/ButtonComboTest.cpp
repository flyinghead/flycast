#include "gtest/gtest.h"

#include "input/mapping.h"

class ButtonComboTest : public ::testing::Test
{
};

TEST_F(ButtonComboTest, intersects__true__non_sequential_overlap)
{
    // Arrange
    InputMapping::ButtonCombo combo1{
        InputMapping::InputSet{
            InputMapping::InputDef::from_button(4993),
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true)
        },
        false
    };
    InputMapping::ButtonCombo combo2{
        InputMapping::InputSet{
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true),
            InputMapping::InputDef::from_button(4993)
        },
        false
    };

    // Act
    bool intersects = combo1.intersects(combo2);

    // Assert
    ASSERT_TRUE(intersects);
}

TEST_F(ButtonComboTest, intersects__false__sequential)
{
    // Arrange
    InputMapping::ButtonCombo combo1{
        InputMapping::InputSet{
            InputMapping::InputDef::from_button(4993),
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true)
        },
        true
    };
    InputMapping::ButtonCombo combo2{
        InputMapping::InputSet{
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true),
            InputMapping::InputDef::from_button(4993)
        },
        true
    };

    // Act
    bool intersects = combo1.intersects(combo2);

    // Assert
    ASSERT_FALSE(intersects);
}

TEST_F(ButtonComboTest, intersects__false__non_sequential_no_overlap)
{
    // Arrange
    InputMapping::ButtonCombo combo1{
        InputMapping::InputSet{
            InputMapping::InputDef::from_button(4993),
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898889, true)
        },
        false
    };
    InputMapping::ButtonCombo combo2{
        InputMapping::InputSet{
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true),
            InputMapping::InputDef::from_button(4993)
        },
        false
    };

    // Act
    bool intersects = combo1.intersects(combo2);

    // Assert
    ASSERT_FALSE(intersects);
}

TEST_F(ButtonComboTest, intersects__true__sequential)
{
    // Arrange
    InputMapping::ButtonCombo combo1{
        InputMapping::InputSet{
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true),
            InputMapping::InputDef::from_button(4993)
        },
        true
    };
    InputMapping::ButtonCombo combo2{
        InputMapping::InputSet{
            InputMapping::InputDef::from_axis(409259, false),
            InputMapping::InputDef::from_axis(898888, true),
            InputMapping::InputDef::from_button(4993)
        },
        true
    };

    // Act
    bool intersects = combo1.intersects(combo2);

    // Assert
    ASSERT_TRUE(intersects);
}
