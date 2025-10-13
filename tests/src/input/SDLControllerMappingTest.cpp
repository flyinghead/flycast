#include "gtest/gtest.h"

#define SDL_GameControllerMapping SDL_GameControllerMapping_MOCKUP
#define SDL_free SDL_free_MOCKUP
#include "sdl/sdl_mappingparser.h"

// mockups
static char mappingUnderTest[1024];
extern "C" {
char *SDL_GameControllerMapping(SDL_GameController *sdlController);
void SDL_free(void *);
}
char *SDL_GameControllerMapping(SDL_GameController *sdlController) {
	return mappingUnderTest;
}
void SDL_free(void *) {
}

class SDLControllerMappingTest : public ::testing::Test
{
};

TEST_F(SDLControllerMappingTest, split_axis)
{
	// full axis lefty split onto 2 different native axes (+a2, -a1)
	strcpy(mappingUnderTest,
			"03000000321500000009000000000000,Razer Serval,+lefty:+a2,-lefty:-a1,a:b0,b:b1,back:b12,"
			"dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b8,"
			"leftx:a0,rightshoulder:b5,rightstick:b9,rightx:a3,righty:a4,start:b7,x:b2,y:b3,");
	SDLControllerMappingParser parser(nullptr);
	SDL_GameControllerButtonBind2 bind;

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_NONE, bind.bindType);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTY, 1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(2, bind.value.axis.axis);
	EXPECT_EQ(1, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTY, -1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(1, bind.value.axis.axis);
	EXPECT_EQ(-1, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTX, 1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(0, bind.value.axis.axis);
	EXPECT_EQ(1, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTX, -1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(0, bind.value.axis.axis);
	EXPECT_EQ(-1, bind.value.axis.direction);
}

TEST_F(SDLControllerMappingTest, reverse_axis)
{
	// inverted full axis (righty:a3~)
	strcpy(mappingUnderTest,
			"03000000260900008888000088020000,Cyber Gadget GameCube Controller,a:b0,b:b1,dpdown:h0.4,"
			"dpleft:h0.8,dpright:h0.2,dpup:h0.1,lefttrigger:a4,leftx:a0,lefty:a1,rightshoulder:b6,"
			"righttrigger:a5,rightx:a2,righty:a3~,start:b7,x:b2,y:b3,");
	SDLControllerMappingParser parser(nullptr);
	SDL_GameControllerButtonBind2 bind;

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_RIGHTY, 1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(3, bind.value.axis.axis);
	EXPECT_EQ(-1, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_RIGHTY, -1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(3, bind.value.axis.axis);
	EXPECT_EQ(1, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(4, bind.value.axis.axis);
	EXPECT_EQ(0, bind.value.axis.direction);
}

TEST_F(SDLControllerMappingTest, reverse_trigger)
{
	// inverted triggers (lefttrigger:a3~ ...)
	strcpy(mappingUnderTest,
			"030000004c0500006802000000000000,PS3 Controller,a:b2,b:b1,back:b9,dpdown:h0.4,"
			"dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b6,leftstick:b10,"
			"lefttrigger:a3~,leftx:a0,lefty:a1,rightshoulder:b7,rightstick:b11,righttrigger:a4~,"
			"rightx:a2,righty:a5,start:b8,x:b3,y:b0,");
	SDLControllerMappingParser parser(nullptr);
	SDL_GameControllerButtonBind2 bind;

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(3, bind.value.axis.axis);
	EXPECT_EQ(2, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(4, bind.value.axis.axis);
	EXPECT_EQ(2, bind.value.axis.direction);
}

TEST_F(SDLControllerMappingTest, triggers_to_full)
{
	// triggers split onto full axis (a3)
	strcpy(mappingUnderTest,
			"03000000a306000022f6000000000000,Cyborg V.3 Rumble Pad,a:b1,b:b2,back:b8,dpdown:h0.4,"
			"dpleft:h0.8,dpright:h0.2,dpup:h0.1,leftshoulder:b4,leftstick:b10,lefttrigger:+a3,leftx:a0,"
			"lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:-a3,rightx:a2,righty:a4,start:b9,x:b0,y:b3,");
	SDLControllerMappingParser parser(nullptr);
	SDL_GameControllerButtonBind2 bind;

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(3, bind.value.axis.axis);
	EXPECT_EQ(1, bind.value.axis.direction);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_AXIS, bind.bindType);
	EXPECT_EQ(3, bind.value.axis.axis);
	EXPECT_EQ(-1, bind.value.axis.direction);
}

TEST_F(SDLControllerMappingTest, hat_and_buttons)
{
	// hat and buttons
	strcpy(mappingUnderTest,
			"050000007e050000062000000f060000,Fake Switch Joy-Con (L),+leftx:h0.2,+lefty:h0.4,"
			"-leftx:h0.8,-lefty:h0.1,a:b2,b:b0,lefttrigger:b4,righttrigger:b5,x:b3,y:b1,");
	SDLControllerMappingParser parser(nullptr);
	SDL_GameControllerButtonBind2 bind;

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_BUTTON, bind.bindType);
	EXPECT_EQ(4, bind.value.button);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_BUTTON, bind.bindType);
	EXPECT_EQ(5, bind.value.button);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTX, 1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_HAT, bind.bindType);
	EXPECT_EQ(0, bind.value.hat.hat);
	EXPECT_EQ(2, bind.value.hat.hat_mask);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTX, -1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_HAT, bind.bindType);
	EXPECT_EQ(0, bind.value.hat.hat);
	EXPECT_EQ(8, bind.value.hat.hat_mask);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTY, 1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_HAT, bind.bindType);
	EXPECT_EQ(0, bind.value.hat.hat);
	EXPECT_EQ(4, bind.value.hat.hat_mask);

	bind = parser.getBindForAxis(SDL_CONTROLLER_AXIS_LEFTY, -1);
	EXPECT_EQ(SDL_CONTROLLER_BINDTYPE_HAT, bind.bindType);
	EXPECT_EQ(0, bind.value.hat.hat);
	EXPECT_EQ(1, bind.value.hat.hat_mask);
}
