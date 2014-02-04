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

/**
 * \file minimal.h Interface for Minimal module example
 */

#include <hncore/modules.h>

//! Plugin's code should be in it's own namespace
namespace MMinimal {

/**
 * Derive a class publically from ModuleBase to implement a module entry point.
 */
class Minimal : public ModuleBase {
	// First argument is the class name of the module's main class,
	// while the second argument is a short, human-readable, descriptive
	// name of the module.
	DECLARE_MODULE(Minimal, "minimal");
public:
	//! Called by core when module is being loaded
	bool onInit();

	//! Called by core when module is being unloaded
	int onExit();
};

}
