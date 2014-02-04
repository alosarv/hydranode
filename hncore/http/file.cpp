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

#include <hncore/http/file.h>
#include <hncore/http/http.h>
#include <hnbase/timed_callback.h>
#include <boost/algorithm/string/erase.hpp>
#include <boost/filesystem/operations.hpp>
#include <fcntl.h>

namespace Http {
namespace Detail {

const std::string TRACE = "http.file";


File::File(SharedFile *sf) :
	m_sf(sf), m_pd(sf->getPartData()), m_md(sf->getMetaData()),
	m_size(sf->getMetaData()->getSize()), m_complete(false),
	m_fd(0), m_file(sf->getPartData()->getDestination()),
	m_tempFile(sf->getPartData()->getLocation()), m_sourceMask(false)
{
	logTrace(TRACE, boost::format("new File(%p)") % this);
}


File::~File() {
	logTrace(TRACE, boost::format("~File(%p)") % this);
	if (m_fd) {
		close(m_fd);
	}
	if (m_pd && m_sourceMask) {
		CHECK_RET(m_size);
		//TODO: this crashes HN...
		//m_pd->delSourceMask(m_size, std::vector<bool>());
		//m_pd->delFullSource(m_size);
	}
}


void File::setSize(uint64_t size) {
	if (m_size) {
		return; //ignore this, if size is already set
	}
	if (size < 1) {
		return tryOpen();
	}

	logTrace(TRACE, boost::format("Setting filesize to: %i") % size);
	m_size = size;
	if (m_md && !m_md->getSize()) {
		m_md->setSize(size);
	}

}


void File::write(const std::string &data, uint64_t offset) try {
	logTrace(TRACE, "write()");
	CHECK_RET(data.size());
	if (!m_fd) {
		tryOpen();
		return write(data, offset);
	}

	CHECK_THROW(m_fd);
	uint64_t ret = lseek64(m_fd, offset, SEEK_SET);
	CHECK_THROW(ret == offset);

	int c = ::write(m_fd, data.c_str(), data.size());
	CHECK_THROW(c == static_cast<int>(data.size()));
	fsync(m_fd);

	logTrace(TRACE,
		boost::format("m_size = %i --> writing[%i-%i]")
		% m_size % offset % (offset + data.size())
	);

	if (offset + data.size() == m_size) {
		logTrace(TRACE, "Download complete.");
		m_complete = true;
		tryComplete();
	}

} catch (std::exception &e) {
	close(m_fd);
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void File::write(
	const std::string &data, uint64_t offset, ::Detail::LockedRangePtr lock
) try {
	CHECK_RET(data.size());
	CHECK_THROW(m_pd->isRunning());
	CHECK_THROW(lock);
	CHECK_THROW(m_size);

	//I'm not quite sure if this might ever occur...
	if (m_md->getSize() != m_pd->getSize()) {
		logDebug(
			boost::format(
				"Possible race-condition detected: filesizes"
				" of MetaData(%i) and PartData(%i) don't match."
			) % m_md->getSize() % m_pd->getSize()
		);
		Utils::timedCallback(
			boost::bind(
				&File::write, this, data, offset, lock
			), 500
		);
		return;
	}

	logTrace(TRACE,
		boost::format("locked[%i-%i] --> writing[%i-%i]")
		% lock->begin() % lock->end()
		% offset % (offset + data.size())
	);

	lock->write(offset, data);

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void File::tryOpen() {
	logTrace(TRACE, "tryOpen()");
	CHECK_RET_MSG(m_fd == 0, "m_fd is already set.");

	m_fd = open(
		m_tempFile.native_file_string().c_str(), O_RDWR|O_BINARY|O_CREAT
	);
	if (m_fd < 1) {
		throw std::runtime_error(
			"Unable to open file " + m_tempFile.native_file_string()
			+ " for writing."
		);
	} else {
		logTrace(TRACE,
			boost::format("Successfully opened file: %s")
			% m_tempFile.native_file_string()
		);
	}
}


void File::tryComplete() try {
	logTrace(TRACE, "tryComplete()");
	if (!m_fd) {
		return; //silently ignore...	
	}

	setSize(boost::filesystem::file_size(m_tempFile));
	m_complete = true;
	Utils::timedCallback(
		boost::bind(&Detail::File::doComplete, this), 1
	);

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void File::doComplete() {
	CHECK_THROW(m_pd);
	CHECK_THROW(m_size);

	m_pd->setComplete(Range64(0, m_size - 1));
	m_pd->tryComplete();
}


void File::setDestination(const std::string &filename) {
	using namespace boost::filesystem;
	std::string tmp = filename;
	CHECK_RET(filename.length() < 255);

	if (!boost::filesystem::native(filename)) {
		logTrace(TRACE,
			boost::format(
				"Filename '%s' is invalid native name, "
				"attempting cleanup..."
			) % filename
		);
		std::string unsafe("\"/\\[]:;|=,^*?~");
		for (uint32_t i = 0; i < unsafe.size(); ++i) {
			boost::algorithm::erase_all(
				tmp, std::string(1, unsafe[i])
			);
		}
		if (boost::filesystem::native(tmp)) {
			logTrace(TRACE,
				boost::format(
					"Name cleanup successful, new "
					"name is '%s'"
				) % tmp
			);
		} else {
			logTrace(TRACE, "Name cleanup failed.");
		}
	}

	if (tmp == m_file.leaf()) {
		return;
	}

	m_file = m_file.branch_path() / path(tmp, native);

	if (m_pd) {
		m_pd->setDestination(m_file);
	}
}


void File::setSourceMask() {
	if (m_pd && m_size) {		
		m_pd->addSourceMask(m_size, std::vector<bool>());
		m_pd->addFullSource(m_size);
		m_sourceMask = true;
	}
}


} // End namespace Detail
} // End namespace Http
