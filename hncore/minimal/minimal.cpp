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
 * \file minimal.cpp Implementation of minimal module example
 */

#include <hncore/minimal/minimal.h>
#include <hnbase/log.h>

namespace MMinimal {

// ImplementModule macro is used to finalize the module implementation.
IMPLEMENT_MODULE(Minimal);

// Module entry point. Return true if your module initialization was
// a success, or false otherwise. If you return false here, the module
// will be unloaded immediately, and onExit() will not be called.
bool Minimal::onInit() {
	logMsg("Minimal Module initializing...");
	return true;
}

// This method is called when this module is about to be unloaded. As
// soon as this function returns, the module will be unloaded.
//
// Important: If your module uses ANY static variables, unloading your
// module will crash the app. As a general rule, if you want your module
// to be unloadable, do not use any static variables, ever.
//
// Currently, this requirement is not enforced (and module unloading is
// disallowed by default), in the future the modules will have a way of
// indicating whether they should be unloadable or not.
//
// The return value indicates exit status of the module, similarly to
// main() method return value. Return 0 to indicate successful shutdown,
// nonzero for error.
int Minimal::onExit() {
	logMsg("Minimal Module unloading.");
	return 0;
}

}
