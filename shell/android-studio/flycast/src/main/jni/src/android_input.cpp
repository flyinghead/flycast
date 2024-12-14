/*
	Copyright 2024 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "android_gamepad.h"
#include "android_keyboard.h"
#include "ui/vgamepad.h"
#include "cfg/option.h"
#include "hw/maple/maple_if.h"

std::shared_ptr<AndroidMouse> mouse;
std::shared_ptr<TouchMouse> touchMouse;
std::shared_ptr<AndroidKeyboard> keyboard;
std::shared_ptr<AndroidVirtualGamepad> virtualGamepad;

extern jobject g_activity;
jmethodID showScreenKeyboardMid;
jmethodID setVGamepadEditModeMid;

//
// VGamepad
//
extern "C" JNIEXPORT jint JNICALL Java_com_flycast_emulator_emu_VGamepad_getVibrationPower(JNIEnv *env, jobject obj) {
	return (jint)config::VirtualGamepadVibration;
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_VGamepad_show(JNIEnv * env, jobject obj) {
	vgamepad::show();
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_VGamepad_hide(JNIEnv * env, jobject obj) {
	vgamepad::hide();
}

extern "C" JNIEXPORT jint JNICALL Java_com_flycast_emulator_emu_VGamepad_hitTest(JNIEnv * env, jobject obj,
		jfloat x, jfloat y) {
	return vgamepad::hitTest(x, y);
}

extern "C" JNIEXPORT jfloat JNICALL Java_com_flycast_emulator_emu_VGamepad_getControlWidth(JNIEnv * env, jobject obj,
		jint controlId) {
	return vgamepad::getControlWidth(static_cast<vgamepad::ControlId>(controlId));
}

extern "C" JNIEXPORT jint JNICALL Java_com_flycast_emulator_emu_VGamepad_layoutHitTest(JNIEnv * env, jobject obj,
		jfloat x, jfloat y) {
	return vgamepad::layoutHitTest(x, y);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_VGamepad_scaleElement(JNIEnv * env, jobject obj,
		jint elemId, jfloat scale) {
	vgamepad::scaleElement(static_cast<vgamepad::Element>(elemId), scale);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_VGamepad_translateElement(JNIEnv * env, jobject obj,
		jint elemId, jfloat x, jfloat y) {
	vgamepad::translateElement(static_cast<vgamepad::Element>(elemId), x, y);
}

namespace vgamepad
{

void setEditMode(bool editing) {
	jni::env()->CallVoidMethod(g_activity, setVGamepadEditModeMid, editing);
}

}

//
// InputDeviceManager
//
extern "C" JNIEXPORT jboolean JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_isMicPluggedIn(JNIEnv *env, jobject obj)
{
	for (const auto& devices : config::MapleExpansionDevices)
		if (static_cast<MapleDeviceType>(devices[0]) == MDT_Microphone
				|| static_cast<MapleDeviceType>(devices[1]) == MDT_Microphone)
			return true;

	return false;
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_init(JNIEnv *env, jobject obj)
{
	input_device_manager = env->NewGlobalRef(obj);
	input_device_manager_rumble = env->GetMethodID(env->GetObjectClass(obj), "rumble", "(IFFI)Z");
	gui_setOnScreenKeyboardCallback([](bool show) {
		if (g_activity != nullptr)
			jni::env()->CallVoidMethod(g_activity, showScreenKeyboardMid, show);
	});
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_joystickAdded(JNIEnv *env, jobject obj,
		jint id, jstring name, jint maple_port, jstring junique_id, jintArray fullAxes, jintArray halfAxes, jboolean hasRumble)
{
	if (id == 0)
		return;
	if (id == AndroidVirtualGamepad::GAMEPAD_ID)
	{
		virtualGamepad = std::make_shared<AndroidVirtualGamepad>(hasRumble);
		GamepadDevice::Register(virtualGamepad);
		touchMouse = std::make_shared<TouchMouse>();
		GamepadDevice::Register(touchMouse);
	}
	else
	{
		std::string joyname = jni::String(name, false);
		std::string unique_id = jni::String(junique_id, false);
		std::vector<int> full = jni::IntArray(fullAxes, false);
		std::vector<int> half = jni::IntArray(halfAxes, false);

		std::shared_ptr<AndroidGamepadDevice> gamepad = std::make_shared<AndroidGamepadDevice>(maple_port, id, joyname.c_str(), unique_id.c_str(), full, half);
		AndroidGamepadDevice::AddAndroidGamepad(gamepad);
		gamepad->setRumbleEnabled(hasRumble);
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_joystickRemoved(JNIEnv *env, jobject obj,
		jint id)
{
	if (id == AndroidVirtualGamepad::GAMEPAD_ID)
	{
		GamepadDevice::Unregister(virtualGamepad);
		virtualGamepad.reset();
		GamepadDevice::Unregister(touchMouse);
		touchMouse.reset();
	}
	else {
		std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
		if (device)
			AndroidGamepadDevice::RemoveAndroidGamepad(device);
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_virtualReleaseAll(JNIEnv *env, jobject obj) {
	if (virtualGamepad)
		virtualGamepad->releaseAll();
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_virtualJoystick(JNIEnv *env, jobject obj,
		jfloat x, jfloat y) {
	if (virtualGamepad)
		virtualGamepad->joystickInput(x, y);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_virtualButtonInput(JNIEnv *env, jobject obj,
		jint controlId, jboolean pressed) {
	if (virtualGamepad)
		virtualGamepad->buttonInput(static_cast<vgamepad::ControlId>(controlId), pressed);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_joystickButtonEvent(JNIEnv *env, jobject obj,
		jint id, jint key, jboolean pressed)
{
	std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
	if (device != NULL)
		return device->gamepad_btn_input(key, pressed);
	else
		return false;
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_keyboardEvent(JNIEnv *env, jobject obj,
		jint key, jboolean pressed)
{
	if (keyboard == nullptr) {
		keyboard = std::make_shared<AndroidKeyboard>();
		GamepadDevice::Register(keyboard);
	}
	keyboard->input(key, pressed);
	return true;
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_keyboardText(JNIEnv *env, jobject obj,
		jint c) {
	gui_keyboard_input((u16)c);
}

static std::map<std::pair<jint, jint>, jint> previous_axis_values;

extern "C" JNIEXPORT jboolean JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_joystickAxisEvent(JNIEnv *env, jobject obj,
		jint id, jint key, jint value)
{
	std::shared_ptr<AndroidGamepadDevice> device = AndroidGamepadDevice::GetAndroidGamepad(id);
	if (device != nullptr)
		return device->gamepad_axis_input(key, value);
	else
		return false;
}

static void createMouse()
{
	if (mouse == nullptr) {
		mouse = std::make_shared<AndroidMouse>(touchMouse == nullptr ? 0 : 1);
		GamepadDevice::Register(mouse);
	}
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_mouseEvent(JNIEnv *env, jobject obj,
		jint xpos, jint ypos, jint buttons)
{
	createMouse();
	mouse->setAbsPos(xpos, ypos, settings.display.width, settings.display.height);
	mouse->setButton(Mouse::LEFT_BUTTON, (buttons & 1) != 0);
	mouse->setButton(Mouse::RIGHT_BUTTON, (buttons & 2) != 0);
	mouse->setButton(Mouse::MIDDLE_BUTTON, (buttons & 4) != 0);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_mouseScrollEvent(JNIEnv *env, jobject obj,
		jint scrollValue)
{
	createMouse();
	mouse->setWheel(scrollValue);
}

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_periph_InputDeviceManager_touchMouseEvent(JNIEnv *env, jobject obj,
		jint xpos, jint ypos, jint buttons)
{
	touchMouse->setAbsPos(xpos, ypos, settings.display.width, settings.display.height);
	touchMouse->setButton(Mouse::LEFT_BUTTON, (buttons & 1) != 0);
	touchMouse->setButton(Mouse::RIGHT_BUTTON, (buttons & 2) != 0);
	touchMouse->setButton(Mouse::MIDDLE_BUTTON, (buttons & 4) != 0);
}
