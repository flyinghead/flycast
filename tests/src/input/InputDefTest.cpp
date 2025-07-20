#include "gtest/gtest.h"

#include <list>
#include <set>

#include "input/mapping.h"

class InputDefTest : public ::testing::Test
{
};

TEST_F(InputDefTest, hash)
{
    // Arrange/Act
    const std::list<u64> hashes = {
        InputMapping::InputDef{0, InputMapping::InputDef::InputType::BUTTON}.get_hash(),
        InputMapping::InputDef{0, InputMapping::InputDef::InputType::AXIS_POS}.get_hash(),
        InputMapping::InputDef{0, InputMapping::InputDef::InputType::AXIS_NEG}.get_hash(),
        InputMapping::InputDef{1, InputMapping::InputDef::InputType::BUTTON}.get_hash(),
        InputMapping::InputDef{2, InputMapping::InputDef::InputType::AXIS_POS}.get_hash(),
        InputMapping::InputDef{3, InputMapping::InputDef::InputType::AXIS_NEG}.get_hash()
    };

    // Assert
    // Ensure all hash values are distinct
    std::set<u64> hashSet;
    for (const u64& hash : hashes)
    {
        hashSet.insert(hash);
    }
    EXPECT_EQ(hashes.size(), hashSet.size());
}

TEST_F(InputDefTest, is_button)
{
    // Arrange
    const InputMapping::InputDef inputDef1{0, InputMapping::InputDef::InputType::BUTTON};
    const InputMapping::InputDef inputDef2{0, InputMapping::InputDef::InputType::AXIS_POS};
    const InputMapping::InputDef inputDef3{0, InputMapping::InputDef::InputType::AXIS_NEG};

    // Act/Assert
    EXPECT_TRUE(inputDef1.is_button());
    EXPECT_FALSE(inputDef2.is_button());
    EXPECT_FALSE(inputDef3.is_button());
}

TEST_F(InputDefTest, is_axis)
{
    // Arrange
    const InputMapping::InputDef inputDef1{0, InputMapping::InputDef::InputType::BUTTON};
    const InputMapping::InputDef inputDef2{0, InputMapping::InputDef::InputType::AXIS_POS};
    const InputMapping::InputDef inputDef3{0, InputMapping::InputDef::InputType::AXIS_NEG};

    // Act/Assert
    EXPECT_FALSE(inputDef1.is_axis());
    EXPECT_TRUE(inputDef2.is_axis());
    EXPECT_TRUE(inputDef3.is_axis());
}

TEST_F(InputDefTest, is_valid__true)
{
    // Arrange
    const InputMapping::InputDef inputDef1{0, InputMapping::InputDef::InputType::BUTTON};
    const InputMapping::InputDef inputDef2{0, InputMapping::InputDef::InputType::AXIS_POS};
    const InputMapping::InputDef inputDef3{0, InputMapping::InputDef::InputType::AXIS_NEG};

    // Act/Assert
    EXPECT_TRUE(inputDef1.is_valid());
    EXPECT_TRUE(inputDef2.is_valid());
    EXPECT_TRUE(inputDef3.is_valid());
}

TEST_F(InputDefTest, is_valid__false__invalid_code)
{
    // Arrange
    const InputMapping::InputDef inputDef{InputMapping::INVALID_CODE, InputMapping::InputDef::InputType::BUTTON};

    // Act/Assert
    EXPECT_FALSE(inputDef.is_valid());
}

TEST_F(InputDefTest, is_valid__false__invalid_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{0, static_cast<InputMapping::InputDef::InputType>(1000)};

    // Act/Assert
    EXPECT_FALSE(inputDef.is_valid());
}

TEST_F(InputDefTest, get_suffix_axis_pos_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{0, InputMapping::InputDef::InputType::AXIS_POS};

    // Act
    const char* suffix = inputDef.get_suffix();

    // Assert
    EXPECT_STREQ(suffix, "+");
}

TEST_F(InputDefTest, get_suffix_axis_neg_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{0, InputMapping::InputDef::InputType::AXIS_NEG};

    // Act
    const char* suffix = inputDef.get_suffix();

    // Assert
    EXPECT_STREQ(suffix, "-");
}

TEST_F(InputDefTest, get_suffix_button_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{0, InputMapping::InputDef::InputType::BUTTON};

    // Act
    const char* suffix = inputDef.get_suffix();

    // Assert
    EXPECT_STREQ(suffix, "");
}

TEST_F(InputDefTest, to_str_axis_pos_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{112233, InputMapping::InputDef::InputType::AXIS_POS};

    // Act
    std::string str = inputDef.to_str();

    // Assert
    EXPECT_EQ(str, "112233+");
}

TEST_F(InputDefTest, to_str_axis_neg_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{112233, InputMapping::InputDef::InputType::AXIS_NEG};

    // Act
    std::string str = inputDef.to_str();

    // Assert
    EXPECT_EQ(str, "112233-");
}

TEST_F(InputDefTest, to_str_button_type)
{
    // Arrange
    const InputMapping::InputDef inputDef{112233, InputMapping::InputDef::InputType::BUTTON};

    // Act
    std::string str = inputDef.to_str();

    // Assert
    EXPECT_EQ(str, "112233");
}

TEST_F(InputDefTest, from_str_axis_pos_type)
{
    // Arrange
    const std::string str = "112233+";

    // Act
    const InputMapping::InputDef inputDef = InputMapping::InputDef::from_str(str);

    // Assert
    EXPECT_EQ(inputDef, (InputMapping::InputDef{112233, InputMapping::InputDef::InputType::AXIS_POS}));
}

TEST_F(InputDefTest, from_str_axis_neg_type)
{
    // Arrange
    const std::string str = "112233-";

    // Act
    const InputMapping::InputDef inputDef = InputMapping::InputDef::from_str(str);

    // Assert
    EXPECT_EQ(inputDef, (InputMapping::InputDef{112233, InputMapping::InputDef::InputType::AXIS_NEG}));
}

TEST_F(InputDefTest, from_str_button_type)
{
    // Arrange
    const std::string str = "112233";

    // Act
    const InputMapping::InputDef inputDef = InputMapping::InputDef::from_str(str);

    // Assert
    EXPECT_EQ(inputDef, (InputMapping::InputDef{112233, InputMapping::InputDef::InputType::BUTTON}));
}

TEST_F(InputDefTest, from_button)
{
    // Act
    const InputMapping::InputDef inputDef = InputMapping::InputDef::from_button(456);

    // Assert
    EXPECT_EQ(inputDef.code, 456);
    EXPECT_EQ(inputDef.type, InputMapping::InputDef::InputType::BUTTON);
}

TEST_F(InputDefTest, from_axis_pos)
{
    // Act
    const InputMapping::InputDef inputDef = InputMapping::InputDef::from_axis(456, true);

    // Assert
    EXPECT_EQ(inputDef.code, 456);
    EXPECT_EQ(inputDef.type, InputMapping::InputDef::InputType::AXIS_POS);
}

TEST_F(InputDefTest, from_axis_neg)
{
    // Act
    const InputMapping::InputDef inputDef = InputMapping::InputDef::from_axis(456, false);

    // Assert
    EXPECT_EQ(inputDef.code, 456);
    EXPECT_EQ(inputDef.type, InputMapping::InputDef::InputType::AXIS_NEG);
}
