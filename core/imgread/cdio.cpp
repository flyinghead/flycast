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
#include "build.h"
#ifdef USE_LIBCDIO
#include "types.h"
#include "imgread/common.h"
#include <cstring>
#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <vector>

namespace hostfs
{

const std::vector<std::string>& getCdromDrives()
{
	static std::vector<std::string> cdromDevices;
	static bool devicesFetched;

	if (devicesFetched)
		return cdromDevices;
	devicesFetched = true;
	// Set a custom log handler
	cdio_log_set_handler([](cdio_log_level_t level, const char message[]) {
		switch (level)
		{
		case CDIO_LOG_DEBUG:
			DEBUG_LOG(GDROM, "%s", message);
			break;
		case CDIO_LOG_INFO:
			INFO_LOG(GDROM, "%s", message);
			break;
		case CDIO_LOG_WARN:
			WARN_LOG(GDROM, "%s", message);
			break;
		case CDIO_LOG_ERROR:
		case CDIO_LOG_ASSERT:
			ERROR_LOG(GDROM, "%s", message);
			break;
		}
	});
	// Get the list of all hardware devices
	char **list = cdio_get_devices(DRIVER_DEVICE);
	if (list != nullptr)
	{
		for (char **dev = &list[0]; *dev != nullptr; dev++)
			cdromDevices.push_back(*dev);
		cdio_free_device_list(list);
	}

	return cdromDevices;
}

}

struct CdioDrive;

struct CdioTrack : public TrackFile
{
	CdioTrack(CdioDrive& disk, bool audio)
		: disk(disk), audio(audio) {}
	bool Read(u32 FAD, u8 *dst, SectorFormat *sector_type, u8 *subcode, SubcodeFormat *subcode_type);

	CdioDrive& disk;
	bool audio;
};

struct CdioDrive : public Disc
{
	bool open(const char *path)
	{
		const std::vector<std::string>& devices = hostfs::getCdromDrives();
		if (!devices.empty())
		{
			// If the list isn't empty, check that an entry exists for the current path
			std::string lpath(path);
#ifdef _WIN32
			if (lpath.substr(0, 4) != "\\\\.\\")
				lpath = "\\\\.\\" + lpath;
#endif
			bool found = false;
			for (const std::string& dev : devices)
				if (dev == lpath) {
					found = true;
					break;
				}
			if (!found) {
				WARN_LOG(GDROM, "%s isn't a CD device", path);
				return false;
			}
		}
		pCdio = cdio_open(path, DRIVER_DEVICE);
		if (pCdio == nullptr) {
			WARN_LOG(GDROM, "Can't open CD device %s", path);
			return false;
		}
		track_t firstTrk = cdio_get_first_track_num(pCdio);
		track_t lastTrk = cdio_get_last_track_num(pCdio);
		if (firstTrk == CDIO_INVALID_TRACK || lastTrk == CDIO_INVALID_TRACK)
		{
			WARN_LOG(GDROM, "Can't find first and/or last track");
			close();
			return false;
		}

		Session session;
		session.StartFAD = 150;
		session.FirstTrack = firstTrk;
		sessions.push_back(session);
		type = CdDA;	// TODO more CD types

		for (int i = firstTrk; i <= lastTrk; i++)
		{
			lba_t lba = cdio_get_track_lba(pCdio, i);
			if (lba == CDIO_INVALID_LBA)
			{
				WARN_LOG(GDROM, "Can't find track %d", i);
				close();
				return false;
			}
			track_format_t format = cdio_get_track_format(pCdio, i);
			bool copy = true;
			if (format == TRACK_FORMAT_AUDIO) {
				track_flag_t copyFlag = cdio_get_track_copy_permit(pCdio, i);
				copy = copyFlag == CDIO_TRACK_FLAG_TRUE;
			}
			else if (!tracks.empty() && !tracks.back().isDataTrack())
			{
				// session 1 lead-out & session 2 lead-in and pre-gap
				tracks.back().EndFAD -= 11400;

				type = CdRom_XA;
				Session session;
				session.StartFAD = lba;
				session.FirstTrack = i;
				sessions.push_back(session);
			}
			Track t;
			t.ADR = 1;			// FIXME correct?
			t.CTRL = format == TRACK_FORMAT_AUDIO ? 0 : CDIO_CDROM_DATA_TRACK;
			t.StartFAD = lba;
			lsn_t last = cdio_get_track_last_lsn(pCdio, i);
			if (last == CDIO_INVALID_LSN)
				WARN_LOG(GDROM, "Can't get last lsn of track %d", i);
			else
				t.EndFAD = cdio_lsn_to_lba(last);
			if (i == firstTrk)
				sessions.front().StartFAD = t.StartFAD;
			INFO_LOG(GDROM, "Track #%d: start %d end %d format %d copy %d", i, t.StartFAD, t.EndFAD, format, copy);
			t.file = new CdioTrack(*this, format == TRACK_FORMAT_AUDIO);
			tracks.push_back(t);
		}
		lba_t leadout = cdio_get_track_lba(pCdio, CDIO_CDROM_LEADOUT_TRACK);
		if (leadout == CDIO_INVALID_LBA)
		{
			WARN_LOG(GDROM, "Can't find leadout track");
			close();
			return false;
		}
		LeadOut.StartFAD = leadout;
		LeadOut.ADR = 1;
		LeadOut.CTRL = CDIO_CDROM_DATA_TRACK;

		return true;
	}

	void close()
	{
		if (pCdio != nullptr) {
			cdio_destroy(pCdio);
			pCdio = nullptr;
		}
	}
	~CdioDrive() {
		close();
	}

	CdIo_t *pCdio = nullptr;
};

bool CdioTrack::Read(u32 FAD, u8 *dst, SectorFormat *sector_type, u8 *subcode, SubcodeFormat *subcode_type)
{
	lsn_t lsn = cdio_lba_to_lsn(FAD);
	if (audio)
	{
		*sector_type = SECFMT_2352;
		if (cdio_read_audio_sector(disk.pCdio, dst, lsn) != DRIVER_OP_SUCCESS) {
			WARN_LOG(GDROM, "Read audio fad %d failed", FAD);
			return false;
		}
	}
	else
	{
		*sector_type = SECFMT_2048_MODE2_FORM1;
		if (cdio_read_mode2_sector(disk.pCdio, dst, lsn, false) != DRIVER_OP_SUCCESS)
		{
			if (cdio_read_mode1_sector(disk.pCdio, dst, lsn, false) != DRIVER_OP_SUCCESS) {
				WARN_LOG(GDROM, "Read data fad %d failed", FAD);
				return false;
			}
			*sector_type = SECFMT_2048_MODE1;
		}
	}

	return true;
}

Disc *cdio_parse(const char *file, std::vector<u8> *digest)
{
	INFO_LOG(GDROM, "Opening CDIO device %s", file);
	CdioDrive *disk = new CdioDrive();

	if (disk->open(file))
	{
		if (digest != nullptr)
			digest->clear();
		return disk;
	}
	else {
		delete disk;
		return nullptr;
	}
}

#endif	// USE_LIBCDIO
