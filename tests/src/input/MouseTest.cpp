#include "gtest/gtest.h"

#include "cfg/option.h"
#include "input/mouse.h"

class MouseTest : public ::testing::Test
{
protected:
    int sensitivity;
    int stretching;
    bool rotate;

    void SetUp() override
    {
        sensitivity = config::MouseSensitivity;
        stretching = config::ScreenStretching;
        rotate = config::Rotate90;
        config::MouseSensitivity = 100;
        config::ScreenStretching = 100;
        config::Rotate90 = false;
        // Initialize the window dimensions used by relative input.
        SetMousePosition(0, 0, 640, 480);
        resetMouseState();
    }

    void TearDown() override
    {
        resetMouseState();
        config::MouseSensitivity = sensitivity;
        config::ScreenStretching = stretching;
        config::Rotate90 = rotate;
    }

    static void resetMouseState()
    {
        for (unsigned port = 0; port < 4; port++)
        {
            mo_x_abs[port] = mo_y_abs[port] = 0;
            mo_x_prev[port] = mo_y_prev[port] = -1;
            mo_x_delta[port] = mo_y_delta[port] = mo_wheel_delta[port] = 0;
        }
    }
};

TEST_F(MouseTest, AbsoluteMotionUsesWindowScaleAndSensitivity)
{
    config::MouseSensitivity = 125;
    SetMousePosition(200, 160, 1280, 960);
    EXPECT_EQ(mo_x_abs[0], 100);
    EXPECT_EQ(mo_y_abs[0], 80);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 0);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 0);

    SetMousePosition(206, 156, 1280, 960);
    SetMousePosition(202, 158, 1280, 960);
    EXPECT_EQ(mo_x_abs[0], 101);
    EXPECT_EQ(mo_y_abs[0], 79);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 1.25f);
    EXPECT_FLOAT_EQ(mo_y_delta[0], -1.25f);
}

TEST_F(MouseTest, RelativeMotionAccumulatesAndUpdatesAbsolutePosition)
{
    SetMousePosition(100, 100, 640, 480);
    config::MouseSensitivity = 50;
    SetRelativeMousePosition(8, -4);
    SetRelativeMousePosition(-2, 6);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 3);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 1);
    EXPECT_EQ(mo_x_abs[0], 103);
    EXPECT_EQ(mo_y_abs[0], 101);
}

TEST_F(MouseTest, ReturningToAbsoluteInputDoesNotReplayConsumedMotion)
{
    SetMousePosition(100, 100, 640, 480);
    SetRelativeMousePosition(100, 40);
    // The guest has already consumed the captured movement.
    mo_x_delta[0] = mo_y_delta[0] = 0;

    SetMousePosition(200, 140, 640, 480);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 0);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 0);
    SetMousePosition(201, 138, 640, 480);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 1);
    EXPECT_FLOAT_EQ(mo_y_delta[0], -2);
}

TEST_F(MouseTest, ReturningToAbsoluteInputPreservesPendingFractions)
{
    SetMousePosition(100, 100, 640, 480);
    config::MouseSensitivity = 25;
    SetRelativeMousePosition(1, -3);
    mo_wheel_delta[0] = 0.5f;

    // A restored host position need not match the emulated absolute position.
    SetMousePosition(300, 200, 640, 480);
    EXPECT_EQ(mo_x_abs[0], 300);
    EXPECT_EQ(mo_y_abs[0], 200);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 0.25f);
    EXPECT_FLOAT_EQ(mo_y_delta[0], -0.75f);
    SetMousePosition(302, 204, 640, 480);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 0.75f);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 0.25f);
    EXPECT_FLOAT_EQ(mo_wheel_delta[0], 0.5f);
}

TEST_F(MouseTest, ChangingInputModeOnlyRebasesItsOwnPort)
{
    SetMousePosition(100, 100, 640, 480, 0);
    SetMousePosition(200, 200, 640, 480, 1);
    SetRelativeMousePosition(20, -10, 0);
    SetMousePosition(203, 204, 640, 480, 1);
    SetMousePosition(120, 90, 640, 480, 0);

    EXPECT_FLOAT_EQ(mo_x_delta[0], 20);
    EXPECT_FLOAT_EQ(mo_y_delta[0], -10);
    EXPECT_FLOAT_EQ(mo_x_delta[1], 3);
    EXPECT_FLOAT_EQ(mo_y_delta[1], 4);
}

TEST_F(MouseTest, ReturningToAbsoluteInputPreservesRotation)
{
    config::Rotate90 = true;
    SetMousePosition(100, 200, 480, 640);
    SetRelativeMousePosition(8, 4);
    EXPECT_FLOAT_EQ(mo_x_delta[0], -4);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 8);
    EXPECT_EQ(mo_x_abs[0], 435);
    EXPECT_EQ(mo_y_abs[0], 108);
    mo_x_delta[0] = mo_y_delta[0] = 0;

    SetMousePosition(108, 204, 480, 640);
    EXPECT_FLOAT_EQ(mo_x_delta[0], 0);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 0);
    SetMousePosition(110, 207, 480, 640);
    EXPECT_FLOAT_EQ(mo_x_delta[0], -3);
    EXPECT_FLOAT_EQ(mo_y_delta[0], 2);
}
