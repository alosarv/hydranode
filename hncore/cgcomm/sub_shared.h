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

#ifndef __SUB_SHARED_H__
#define __SUB_SHARED_H__

#include <hncore/cgcomm/subsysbase.h>
#include <hncore/cgcomm/opcodes.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>

namespace CGComm {
namespace Subsystem {

class Shared : public SubSysBase {
public:
	Shared(boost::function<void (const std::string&)> sendFunc);
	virtual void handle(std::istream &i);

	class CacheEntry {
	public:
		CacheEntry(SharedFile *file);
		void update(SharedFile *file);
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
		SharedFile* getFile() const { return m_file; }
	public: // public for access by multi_index
		SharedFile *m_file;
		uint32_t    m_id;
		std::string m_name;
		uint64_t    m_size;
		std::string m_location;
		uint32_t    m_upSpeed;
		uint64_t    m_uploaded;
		uint32_t    m_partDataId;
		bool        m_dirty;
		bool        m_zombie;
		std::vector<uint32_t> m_children;
	};
private:
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
					CacheEntry, SharedFile*, 
					&CacheEntry::m_file
				>
			>
		>
	> Cache;
	typedef Cache::nth_index<0>::type::iterator CIter;
	typedef Cache::nth_index<1>::type::iterator IDIter;
	typedef Cache::nth_index<2>::type::iterator FIter;

	void monitor(std::istream &i);
	void addShared(std::istream &i);
	void remShared(std::istream &i);
	void sendList();
	void rebuildCache();
	void onEvent(SharedFile *file, int event);
	void onMonitorTimer();
	void changeId(FIter i);

	Cache m_cache;
	uint32_t m_updateTimer;
};

}
}
#endif
