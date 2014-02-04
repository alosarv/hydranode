/*
 *  Copyright (C) 2004-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#ifndef __ED2K_TYPES_H__
#define __ED2K_TYPES_H__

/**
 * \file ed2ktypes.h Types related to ED2K module
 */

#include <hncore/ed2k/fwd.h>
#include <hnbase/osdep.h>
#include <hncore/fwd.h>

namespace Donkey {

enum ED2K_Constants {
	/**
	 * The size of a single chunk. This indicates the maximum length of a
	 * chunk requested from another client. Smaller chunks may be requested
	 * if neccesery.
	 */
	ED2K_CHUNKSIZE = 180*1024
};

//! @name Socket types
//@{
typedef SSocket<ED2K, Socket::Server, Socket::TCP> ED2KServerSocket;
typedef SSocket<ED2K, Socket::Client, Socket::TCP> ED2KClientSocket;
// uncomment when SSocket/Scheduler API supports udp sockets
//typedef SSocket<ED2K, Socket::Server, Socket::UDP> ED2KUDPSocket;
typedef UDPSocket ED2KUDPSocket;
//@}

//! SecIdentState State values
enum SecIdentState {
	SI_SIGNEEDED       = 0x01, //!< Signature is needed
	SI_KEYANDSIGNEEDED = 0x02  //!< Public key AND signature is needed
};

//! Types of IP sent in Signature packet; defined here since it's also used in
//! CreditsDb::verifySignature() method
enum IpTypeValues {
	IP_REMOTE = 10,  //!< Receiving party's IP address is included
	IP_LOCAL  = 20,  //!< Sending party's IP address is included
	IP_NONE   = 30   //!< No ip address is included in signature
};

typedef uint8_t IpType;

//! Utility - checks whether an ID is low
inline bool isLowId(const uint32_t &id) { return id < 0x00ffffff; }
//! Utility - checks whether an ID is high
inline bool isHighId(const uint32_t &id) { return id > 0x00ffffff; }

#define COL_SEND COL_CYAN
#define COL_RECV COL_GREEN
#define COL_COMP COL_BYELLOW

} // end namespace Donkey

#endif
