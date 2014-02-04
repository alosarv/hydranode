/*
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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

#ifndef __ED2K_KAD_OPCODES_H__
#define __ED2K_KAD_OPCODES_H__

namespace ED2KKad {
	namespace Opcode {
		enum ED2KKadOpcodes {
			BOOTSTRAP_REQ = 0,
			BOOTSTRAP_RES = 0x8,

			HELLO_REQ = 0x10,
			HELLO_RES = 0x18
		};
	}
}

#endif
