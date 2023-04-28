/*
	Copyright 2021 flyinghead

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
#pragma once
#include "types.h"
#include <future>

struct MapleInputState;

namespace ggpo
{
struct NetworkStats {
   struct {
      int   send_queue_len;
      int   recv_queue_len;
      int   ping;
      int   kbps_sent;
      int   recv_packet_loss;
   } network;
   struct {
      int   local_frames_behind;
      int   remote_frames_behind;
   } timesync;
   struct {
	   int predicted_frames;
   } sync;
   struct {
	   int total_rollbacked_frames;
	   int total_timesync;
   } extra;
};

std::future<bool> startNetwork();
void startSession(int localPort, int localPlayerNum);
void stopSession();
void getInput(MapleInputState inputState[4]);
bool nextFrame();
bool active();
void displayStats();
void endOfFrame();
bool getCurrentFrame(int* frame);
void sendChatMessage(int playerNum, const std::string& msg);
void receiveChatMessages(void (*callback)(int playerNum, const std::string& msg));
bool isConnected(int playerNum);
void disconnect(int playerNum);
void randomInput(bool enable, u64 seed, u32 inputMask);
void getNetworkStats(int playerNum, NetworkStats* stats);
std::future<bool> gdxsvStartNetwork(
    const char* sessionCode, int me,
    const std::vector<std::string>& ips,
    const std::vector<u16>& ports,
    const std::vector<u8>& relays);

static inline bool rollbacking() {
	extern bool inRollback;

	return inRollback;
}

static inline void setExInput(u16 exInput) {
	extern u16 localExInput;
	localExInput = exInput;
}

}
