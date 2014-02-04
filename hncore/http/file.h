/*
 *  Copyright (C) 2005-2006 Gaubatz Patrick <patrick@gaubatz.at>
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

#ifndef __FILE_H__
#define __FILE_H__

#include <string>
#include <hnbase/object.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hncore/metadata.h>
#include <hncore/fileslist.h>
#include <boost/filesystem/path.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace Http {
namespace Detail {

/**
 * @brief File
 */
class File :
	public Trackable,
	public boost::enable_shared_from_this<File>
{
public:
	File(SharedFile *sf);
	~File();

	PartData*   getPartData()   { return m_pd; }
	SharedFile* getSharedFile() { return m_sf; }
	MetaData*   getMetaData()   { return m_md; }
	uint64_t    getSize()       { return m_size; }

	void setSize(uint64_t size);
	void setComplete() { m_complete = true; }
	void setSourceMask();

	//! Try to complete the download when the download is in MODE_FILE mode
	void tryComplete();

	//! Checks if the download is complete
	bool isComplete() { return m_complete; }

	/**
	 * Sets the destination filename.
	 *
	 * @note "Unsafe" characters in the filename, e.g. ~, ?, ...
	 *       are removed from the filename to ensure cross-platform
	 *       compatibility.
	 */
	void setDestination(const std::string &filename);

	void write(const std::string &data, uint64_t offset);
	void write(
		const std::string &data,
		uint64_t offset,
		::Detail::LockedRangePtr lock
	);

private:
	File(); //<! Forbidden

	//! Try to open the temporary file. (will be created if non-existant)
	void tryOpen();

	void doComplete();

	SharedFile *m_sf;
	PartData   *m_pd;
	MetaData   *m_md;
	uint64_t    m_size;
	bool        m_complete;
	int         m_fd;
	boost::filesystem::path m_file;
	boost::filesystem::path m_tempFile;

	/**
	 * "True" when PartData's addFullSource() and addSourceMask()
	 * functions have been used.
	 * This is necessary to prevent delSourceMask() from being called twice.
	 */
	bool m_sourceMask;
};

} // End namespace Detail
} // End namespace Http

#endif
