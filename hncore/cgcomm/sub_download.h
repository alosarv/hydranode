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

#ifndef __SUB_DOWNLOAD_H__
#define __SUB_DOWNLOAD_H__

#include <hncore/cgcomm/subsysbase.h>
#include <hncore/cgcomm/opcodes.h>
#include <hncore/partdata.h>
#include <hnbase/event.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

namespace CGComm {
namespace Subsystem {

inline uint32_t getFId(PartData *file) {
	if (file->getMetaData()) {
		return getStableId(file->getMetaData());
	} else {
		return file->getId();
	}
}

class Download : public SubSysBase {
public:
	Download(boost::function<void (const std::string&)> sendFunc);
	virtual void handle(std::istream &i);

	class CacheEntry {
	public:
		CacheEntry(PartData *file);
		void update(
			PartData *file,
			FileState newState = ::CGComm::DSTATE_RUNNING
		);
		void setDirty(bool state) { m_dirty = state; }
		bool isDirty() const { return m_dirty; }
		void setZombie() { m_zombie = true; }
		bool isZombie() const { return m_zombie; }
		bool operator==(const CacheEntry &x) const;
		friend std::ostream& operator<<(
			std::ostream &o, const CacheEntry &c
		);

		std::string getName() const { return m_name; }
		uint32_t    getId()   const { return m_id;   }
		PartData*   getFile() const { return m_file; }
	public: // public for access by multi_index
		PartData    *m_file;
		uint32_t     m_id;
		FileState    m_state;
		std::string  m_name;
		uint64_t     m_size;
		std::string  m_location;
		std::string  m_destination;
		uint32_t     m_sourceCnt;
		uint32_t     m_fullSourceCnt;
		uint64_t     m_completed;
		uint32_t     m_speed;
		uint8_t      m_avail;
		bool         m_dirty;
		bool         m_zombie;
		std::vector<uint32_t> m_children;
	};
private:
	/**
	 * @name Various packet handlers
	 */
	//@{
	void monitor(std::istream &i);
	void pause(std::istream &i);
	void stop(std::istream &i);
	void resume(std::istream &i);
	void cancel(std::istream &i);
	void getLink(std::istream &i);
	void getFile(std::istream &i);
	void import(std::istream &i);
	void getNames(std::istream &i);
	void getComments(std::istream &i);
	void setName(std::istream &i);
	void setDest(std::istream &i);
	void getLinks(std::istream &i);
	//@}

	/**
	 * Sends the entire current list to GUI.
	 */
	void sendList();

	/**
	 * Rebuilds m_cache completely
	 */
	void rebuildCache();

	/**
	 * Event handler for PartData events, set up for all PartData objects.
	 */
	void onEvent(PartData *file, int event);

	/**
	 * Timed callback handler, sends updates to GUI
	 */
	void onMonitorTimer();

	typedef boost::multi_index_container<
		CacheEntry*,
		boost::multi_index::indexed_by<
			boost::multi_index::hashed_unique<
				boost::multi_index::identity<CacheEntry*>
			>,
			boost::multi_index::hashed_unique<
				boost::multi_index::member<
					CacheEntry, uint32_t, &CacheEntry::m_id
				>
			>,
			boost::multi_index::hashed_unique<
				boost::multi_index::member<
					CacheEntry, PartData*,
					&CacheEntry::m_file
				>
			>
		>
	> Cache;
	typedef Cache::nth_index<0>::type::iterator CIter;
	typedef Cache::nth_index<1>::type::iterator IDIter;
	typedef Cache::nth_index<2>::type::iterator FIter;
	Cache m_cache;

	/**
	 * Updates interval timer, in milliseconds
	 */
	uint32_t m_updateTimer;
};

}
}

#endif
