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

#ifndef __SUB_SEARCH_H__
#define __SUB_SEARCH_H__

#include <hncore/cgcomm/subsysbase.h>
#include <hncore/fwd.h>
#include <boost/signals.hpp>

namespace CGComm {
namespace Subsystem {

class Search : public SubSysBase {
public:
	Search(boost::function<void (const std::string&)> sendFunc);
	~Search();
	virtual void handle(std::istream &i);

	class CacheEntry {
	public:
		CacheEntry(uint32_t id, SearchResultPtr res);

		void update(SearchResultPtr res);
		bool isDirty() const { return m_dirty; }
		void setDirty(bool s) { m_dirty = s; }

		friend std::ostream& operator<<(
			std::ostream &o, const CacheEntry &c
		);
	private:
		uint32_t    m_id;
		std::string m_name;
		uint64_t    m_size;
		uint32_t    m_sourceCnt;
		uint32_t    m_fullSourceCnt;
		StreamData *m_streamData;
		bool        m_dirty;
	};
private:
	void perform(std::istream &i);
	void download(std::istream &i);
//	void foundResults(SearchPtr ptr);
	void sendUpdates();
	SearchPtr m_currentSearch;
	std::vector<CacheEntry*> m_cache;
	uint32_t m_lastResultCount;
};

}
}
#endif
