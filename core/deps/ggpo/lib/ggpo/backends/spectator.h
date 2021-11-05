/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _SPECTATOR_H
#define _SPECTATOR_H

#include "../ggpo_poll.h"
#include "../ggpo_types.h"
#include "sync.h"
#include "backend.h"
#include "timesync.h"
#include "network/udp_proto.h"

#define SPECTATOR_FRAME_BUFFER_SIZE    64

class SpectatorBackend : public IQuarkBackend, IPollSink, Udp::Callbacks {
public:
   SpectatorBackend(GGPOSessionCallbacks *cb, const char *gamename, uint16 localport, int num_players, int input_size, char *hostip, u_short hostport,
		   const void *verification, int verification_size);
   virtual ~SpectatorBackend();


public:
   GGPOErrorCode DoPoll(int timeout) override;
   GGPOErrorCode AddPlayer(GGPOPlayer *player, GGPOPlayerHandle *handle) override { return GGPO_ERRORCODE_UNSUPPORTED; }
   GGPOErrorCode AddLocalInput(GGPOPlayerHandle player, void *values, int size) override { return GGPO_OK; }
   GGPOErrorCode SyncInput(void *values, int size, int *disconnect_flags) override;
   GGPOErrorCode IncrementFrame(void) override;
   GGPOErrorCode DisconnectPlayer(GGPOPlayerHandle handle) override { return GGPO_ERRORCODE_UNSUPPORTED; }
   GGPOErrorCode GetNetworkStats(GGPONetworkStats *stats, GGPOPlayerHandle handle) override { return GGPO_ERRORCODE_UNSUPPORTED; }
   GGPOErrorCode SetFrameDelay(GGPOPlayerHandle player, int delay) override { return GGPO_ERRORCODE_UNSUPPORTED; }
   GGPOErrorCode SetDisconnectTimeout(int timeout) override { return GGPO_ERRORCODE_UNSUPPORTED; }
   GGPOErrorCode SetDisconnectNotifyStart(int timeout) override { return GGPO_ERRORCODE_UNSUPPORTED; }

public:
   void OnMsg(sockaddr_in &from, UdpMsg *msg, int len) override;

protected:
   void PollUdpProtocolEvents(void);
   void CheckInitialSync(void);

   void OnUdpProtocolEvent(UdpProtocol::Event &e);

protected:
   GGPOSessionCallbacks  _callbacks;
   Poll                  _poll;
   Udp                   _udp;
   UdpProtocol           _host;
   bool                  _synchronizing;
   int                   _input_size;
   int                   _num_players;
   int                   _next_input_to_send;
   GameInput             _inputs[SPECTATOR_FRAME_BUFFER_SIZE];
};

#endif
