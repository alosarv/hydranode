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

#ifndef __THREADSAFE_PTRS_H__
#define __THREADSAFE_PTRS_H__

#include <hnbase/osdep.h>
#include <boost/thread.hpp>
#include <map>

namespace boost {
	MSVC_ONLY(extern "C" {)
	extern HNBASE_EXPORT std::map<void*, uint64_t> g_tsRefCounter;
	extern HNBASE_EXPORT boost::recursive_mutex g_tsRefCounterMutex;
	MSVC_ONLY(})

	template<typename T>
	inline void updateRefs(T *t, bool add) {
		boost::recursive_mutex::scoped_lock l(g_tsRefCounterMutex);
		if (add) {
			++g_tsRefCounter[t];
		} else {
			std::map<void*, uint64_t>::iterator it(
				g_tsRefCounter.find(t)
			);
			assert(it->second);
			--it->second;
			if (!it->second) {
				delete reinterpret_cast<T*>(it->first);
				g_tsRefCounter.erase(it);
			}
		}
	}
	template<typename T>
	inline void intrusive_ptr_add_ref(T *t) {
		updateRefs(t, true);
	}

	template<typename T>
	inline void intrusive_ptr_release(T *t) {
		updateRefs(t, false);
	}
}

#endif
