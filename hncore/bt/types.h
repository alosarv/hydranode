/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file types.h   Forward declaration of various BT module types
 */

#ifndef __BT_TYPES_H__
#define __BT_TYPES_H__

#include <hnbase/ssocket.h>
#include <hnbase/utils.h>

#ifdef __BT_IMPORTS__
	#define BTEXPORT IMPORT
#else
	#define BTEXPORT EXPORT
#endif

namespace Bt {
	class TorrentFile;
	class PartialTorrent;
	class Client;
	class BitTorrent;
	class TorrentInfo;
	class Torrent;
	typedef SSocket<BitTorrent, Socket::Client, Socket::TCP> TcpSocket;
	typedef SSocket<BitTorrent, Socket::Server, Socket::TCP> TcpListener;
	// Bittorrent protocol is big-endian, thus define big-endian streams
	typedef Utils::EndianStream<std::istringstream, BIG_ENDIAN> BEIStream;
	typedef Utils::EndianStream<std::ostringstream, BIG_ENDIAN> BEOStream;

	enum CustomTorrentFileEvent {
		EVT_CHILDDESTROYED = -1     //!< Emitted by TorrentFile
	};
}

#endif
