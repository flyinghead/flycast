/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "backends/p2p.h"
#include "backends/synctest.h"
#include "backends/spectator.h"
#include "ggpo_types.h"
#include "ggponet.h"

struct Init
{
	Init() {
		srand(GGPOPlatform::GetCurrentTimeMS() + GGPOPlatform::GetProcessID());
#ifdef _WIN32
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 0), &wsaData);
#endif
	}
};
static Init init;

void
ggpo_log(GGPOSession *ggpo, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   ggpo_logv(ggpo, fmt, args);
   va_end(args);
}

void
ggpo_logv(GGPOSession *ggpo, const char *fmt, va_list args)
{
   if (ggpo)
      ggpo->Logv(fmt, args);
}

GGPOErrorCode
ggpo_start_session(GGPOSession **session,
                   GGPOSessionCallbacks *cb,
                   const char *game,
                   int num_players,
                   int input_size,
                   unsigned short localport,
				   const void *verification,
				   int verification_size)
{
	try {
	   *session= (GGPOSession *)new Peer2PeerBackend(cb,
													 game,
													 localport,
													 num_players,
													 input_size,
													 verification,
													 verification_size);
	   return GGPO_OK;
	} catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_start_session: %s", e.what());
	   return e.ggpoError;
	}
}

GGPOErrorCode
ggpo_add_player(GGPOSession *ggpo,
                GGPOPlayer *player,
                GGPOPlayerHandle *handle)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->AddPlayer(player, handle);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_add_player: %s", e.what());
	   return e.ggpoError;
   }
}



GGPOErrorCode
ggpo_start_synctest(GGPOSession **ggpo,
                    GGPOSessionCallbacks *cb,
                    const char *game,
                    int num_players,
                    int input_size,
                    int frames)
{
	try {
	   *ggpo = (GGPOSession *)new SyncTestBackend(cb, game, frames, num_players);
	   return GGPO_OK;
	} catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_start_synctest: %s", e.what());
	   return e.ggpoError;
	}
}

GGPOErrorCode
ggpo_set_frame_delay(GGPOSession *ggpo,
                     GGPOPlayerHandle player,
                     int frame_delay)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->SetFrameDelay(player, frame_delay);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_set_frame_delay: %s", e.what());
	   return e.ggpoError;
   }
}

GGPOErrorCode
ggpo_idle(GGPOSession *ggpo, int timeout)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->DoPoll(timeout);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_idle: %s", e.what());
	   return e.ggpoError;
   }
}

GGPOErrorCode
ggpo_add_local_input(GGPOSession *ggpo,
                     GGPOPlayerHandle player,
                     void *values,
                     int size)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
      return ggpo->AddLocalInput(player, values, size);
   } catch (const GGPOException& e) {
      Log("GGPOException in ggpo_add_local_input: %s", e.what());
      return e.ggpoError;
   }
}

GGPOErrorCode
ggpo_synchronize_input(GGPOSession *ggpo,
                       void *values,
                       int size,
                       int *disconnect_flags)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
      return ggpo->SyncInput(values, size, disconnect_flags);
   } catch (const GGPOException& e) {
      Log("GGPOException in ggpo_synchronize_input: %s", e.what());
      return e.ggpoError;
   }
}

GGPOErrorCode ggpo_disconnect_player(GGPOSession *ggpo,
                                     GGPOPlayerHandle player)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->DisconnectPlayer(player);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_disconnect_player: %s", e.what());
	   return e.ggpoError;
   }
}

GGPOErrorCode
ggpo_advance_frame(GGPOSession *ggpo)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->IncrementFrame();
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_advance_frame: %s", e.what());
	   return e.ggpoError;
   }
}

GGPOErrorCode
ggpo_get_network_stats(GGPOSession *ggpo,
                       GGPOPlayerHandle player,
                       GGPONetworkStats *stats)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->GetNetworkStats(stats, player);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_get_network_stats: %s", e.what());
	   return e.ggpoError;
   }
}


GGPOErrorCode
ggpo_close_session(GGPOSession *ggpo)
{
   if (!ggpo) {
      return GGPO_ERRORCODE_INVALID_SESSION;
   }
   delete ggpo;
   return GGPO_OK;
}

GGPOErrorCode
ggpo_set_disconnect_timeout(GGPOSession *ggpo, int timeout)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->SetDisconnectTimeout(timeout);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_set_disconnect_timeout: %s", e.what());
	   return e.ggpoError;
   }
}

GGPOErrorCode
ggpo_set_disconnect_notify_start(GGPOSession *ggpo, int timeout)
{
   if (!ggpo)
      return GGPO_ERRORCODE_INVALID_SESSION;
   try {
	   return ggpo->SetDisconnectNotifyStart(timeout);
   } catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_set_disconnect_notify_start: %s", e.what());
	   return e.ggpoError;
   }
}

GGPOErrorCode ggpo_start_spectating(GGPOSession **session,
                                    GGPOSessionCallbacks *cb,
                                    const char *game,
                                    int num_players,
                                    int input_size,
                                    unsigned short local_port,
                                    char *host_ip,
                                    unsigned short host_port,
									const void *verification,
									int verification_size)
{
	try {
	   *session= (GGPOSession *)new SpectatorBackend(cb,
													 game,
													 local_port,
													 num_players,
													 input_size,
													 host_ip,
													 host_port,
													 verification,
													 verification_size);
	   return GGPO_OK;
	} catch (const GGPOException& e) {
	   Log("GGPOException in ggpo_start_spectating: %s", e.what());
	   return e.ggpoError;
	}
}

GGPOErrorCode ggpo_send_message(GGPOSession *ggpo,
                                const void *msg,
                                int len,
								bool spectators)
{
	if (ggpo == nullptr)
		return GGPO_ERRORCODE_INVALID_SESSION;
	return ggpo->SendMessage(msg, len, spectators);
}

