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
 * \file clientext.h Extension classes for Client object
 */

#ifndef __ED2K_CLIENTEXT_H__
#define __ED2K_CLIENTEXT_H__

#include <hncore/ed2k/fwd.h>
#include <hncore/ed2k/downloadlist.h>
#include <hnbase/hash.h>
#include <hnbase/range.h>
#include <hncore/fwd.h>
#include <boost/tuple/tuple.hpp>

namespace Donkey {
namespace Detail {

//! PartMaps are shared between SourceInfo and DownloadInfo
typedef boost::shared_ptr<std::vector<bool> > PartMapPtr;


/**
 * Base class for "Client Extensions" - state-dependant parts of Client
 * constructed as needed; Encapsulates m_parent pointer, which the extension
 * can use to access the parent object. This is mainly used for trace logging,
 * but also for Download::addSource() calls.
 */
class ClientExtBase {
public:
	ClientExtBase(Client *parent) : m_parent(parent) {}
	void setParent(Client *newParent) { m_parent = newParent; }
protected:
	Client *m_parent;
private:
	ClientExtBase();
	ClientExtBase(const ClientExtBase&);
	ClientExtBase& operator=(const ClientExtBase&);
};

/**
 * UploadInfo object is a part of Client object that represents the upload
 * state. Each uploading client is required to have this object as member. This
 * object contains all the information needed to perform actual uploading to the
 * remote client, e.g. data buffers et al.
 */
class UploadInfo : public ClientExtBase {
public:
	UploadInfo(Client *parent, QueueInfoPtr nfo);
	~UploadInfo();

	/**
	 * @name Accessors for public usage
	 */
	//@{
	SharedFile*    getReqFile()   const { return m_reqFile;          }
	Hash<ED2KHash> getReqHash()   const { return m_reqHash;          }
	uint32_t       getSent()      const { return m_sent;             }
	bool           isCompressed() const { return m_compressed;       }
	bool           hasBuffered()  const { return m_buffer.size();    }
	uint8_t   getReqChunkCount()  const { return m_reqChunks.size(); }

	void setReqFile(SharedFile *sf)          { m_reqFile = sf;     }
	void setReqHash(const Hash<ED2KHash> &h) { m_reqHash = h;      }
	void addReqChunk(Range32 r);
	//@}

	/**
	 * Reads first chunk in m_reqChunks into m_buffer from disk
	 */
	void bufferData();

	/**
	 * Attempts to compress the current m_buffer.
	 *
	 * @return true if compression succeeded, false otherwise
	 */
	bool compress();

	/**
	 * Get next data chunk from m_buffer. The requested data is
	 * deleted from buffer.
	 *
	 * @param amount     Amount of data to retrieve
	 * @return           Three values, where first integer is the begin
	 *                   offset of the data, second value is either end
	 *                   offset (in case of uncompressed data), or the size
	 *                   of total compressed chunk, and string containing
	 *                   the actual data.
	 *
	 * \note This function may return less data than requested if there was
	 *       not enough data in m_buffer.
	 */
	boost::tuple<uint32_t, uint32_t, std::string> getNext(uint32_t amount);

	/**
	 * Connects speedmeter to parent client's socket
	 */
	void connectSpeeder();

	//! \returns Number of UploadInfo objects alive
	static size_t count();
private:
	//! Requested file
	SharedFile *m_reqFile;

	//! The hash of the requested file
	Hash<ED2KHash> m_reqHash;

	//! Requested ranges
	std::list<Range32> m_reqChunks;

	/**
	 * Data to be sent to this client, buffered. This data may be compressed
	 * if the remote client supports compression. Also, the size of this
	 * buffer may be up to 180k when upload has just started, so this is
	 * quite memory-heavy.
	 */
	std::string m_buffer;

	/**
	 * If the client is uploading, this is the current uploading position.
	 */
	uint32_t m_curPos;

	/**
	 * End position of current buffered data. Notice that this may instead
	 * indicate the size of compressed chunk, instead of the end offset, if
	 * the buffer is compressed. This because of eMule extended protocol
	 * system, refer to Packets::PackedChunk packet implementation for more
	 * information.
	 */
	uint32_t m_endPos;

	/**
	 * Whether current buffer is compressed or not.
	 */
	bool m_compressed;

	/**
	 * How much data (excluding overhead) have we sent to this client during
	 * this session. We should send 9.28mb to every client and then
	 * terminate the uploading so others can also get data.
	 *
	 * This counter thus starts at 0, updated at every getNext() call, and
	 * if reaches 9.28mb, the object should be destroyed.
	 */
	uint32_t m_sent;

	/**
	 * Connected to SharedFile::getUpSpeed signal
	 */
	boost::signals::scoped_connection m_speeder;
};

/**
 * QueueInfo object is part of Client object that represents the queue state.
 * This object is an extension of the Client object and contains data members
 * related to queued client only. Each client in waiting queue must have this as
 * member object.
 */
class QueueInfo : public ClientExtBase {
public:
	QueueInfo(
		Client *parent, SharedFile *sf, const Hash<ED2KHash> &h
	);
	QueueInfo(Client *parent, UploadInfoPtr nfo);
	~QueueInfo();

	/**
	 * @name Accessors for public usage
	 */
	//!@{
	uint32_t       getQR()         const { return m_queueRanking;  }
	SharedFile*    getReqFile()    const { return m_reqFile;       }
	Hash<ED2KHash> getReqHash()    const { return m_reqHash;       }
	uint64_t    getWaitStartTime() const { return m_waitStartTime; }
	uint64_t   getLastQueueReask() const { return m_lastQueueReask;}

	void setQR(uint32_t r)                   { m_queueRanking = r; }
	void setReqFile(SharedFile *sf, const Hash<ED2KHash> &h) {
		m_reqFile = sf;
		m_reqHash = h;
	}
	void resetWaitTime()     { m_waitStartTime = Utils::getTick(); }
	//!@}

	//! \returns Number of QueueInfo objects alive
	static size_t count();
private:
	friend class ::Donkey::Client;

	/**
	 * Queue ranking. 0 means we are currently uploading to the client.
	 * Nonzero means we are in waiting queue, on that specific position.
	 * This member is updated by ClientList class during queue resorting.
	 */
	uint32_t m_queueRanking;

	/**
	 * The requested file. eMule-compatible clients may change this while
	 * waiting in the queue.
	 */
	SharedFile *m_reqFile;

	/**
	 * Hash of the requested file; stored here to lower MetaDb lookups
	 * during reasks
	 */
	Hash<ED2KHash> m_reqHash;

	/**
	 * Tick when the client entered queue. This is set to current tick when
	 * the object is constructed, and is used in queue score calculations
	 */
	uint64_t m_waitStartTime;

	/**
	 * Last time this client asked US about QR. If this is longer than 1
	 * hour from current tick, the client will be dropped from queue during
	 * next queue update.
	 */
	uint64_t m_lastQueueReask;
};

/**
 * DownloadInfo object controls downloading data from the remote client. It is
 * driven by Client object, and instanciated when we can start downloading from
 * the remote client. As such, it's purpose is to act as a layer between Client
 * (which only performs basic packet handling), and actual data processing
 * (which is handled by PartData class internally). This class is also
 * responsible for generating chunk requests for asking from the remote client.
 */
class DownloadInfo : public ClientExtBase {
public:
	DownloadInfo(Client *parent, PartData *pd, PartMapPtr partMap);
	~DownloadInfo();

	PartData* getReqPD() const { return m_reqPD; }

	/**
	 * Generate three chunk requests to be downloaded from this client. Call
	 * this method AFTER selecting a part using selectPart() method.
	 *
	 * Note that in ed2k network, chunk requests are used in overlapping
	 * manner, e.g. each next chunk request contains two previous ranges
	 * plus one new range, and a new chunk request is sent after each chunk
	 * is downloaded. This method takes care of these details, and returns
	 * three chunk requests ready to be sent out to the uploader.
	 *
	 * @return    Up to three requested chunks of size <= 180kb each
	 */
	std::list<Range32> getChunkReqs();

	/**
	 * Write data to the underlying temp file.
	 *
	 * @param r       Range to write to
	 * @param data    Data to be written
	 * @return        True if chunk was completed, and
	 *                getReqChunks() must be called again.
	 */
	bool write(Range32 r, const std::string &data);

	/**
	 * Write packed data.
	 *
	 * @param begin   Begin offset of the packed data.
	 * @param size    Total size of packed data, e.g. at what point we
	 *                consider the data buffer "complete".
	 * @param data    The data itself
	 * @return        True if chunk was completed, and getReqChunks() must
	 *                be called again.
	 */
	bool writePacked(uint32_t begin, uint32_t size,const std::string &data);

	//! \returns Number of DownloadInfo objects alive
	static size_t count();
private:
	typedef std::list< ::Detail::LockedRangePtr >::iterator Iter;

	PartData         *m_reqPD;   //!< Requested file

	//!< The chunks the remote party has of this file
	PartMapPtr m_partMap;

	::Detail::UsedRangePtr m_curPart;   //!< Current active <=9500kb part

	//! Currently locked up to 3 chunks
	std::list< ::Detail::LockedRangePtr > m_reqChunks;

	std::string m_packedBuffer; //!< Internal buffer for packed data
	uint32_t m_packedBegin;     //!< Begin offset for packed data

	//! How much data we have received during this download session
	uint32_t m_received;

	//! Connection between socket's speedmeter and PartData getSpeed signal
	boost::signals::connection m_speeder;
};

/**
 * SourceInfo indicates a client which has one (or more) files to offer
 * to us. SourceInfo object remains alive until the client has some
 * files to offer us, e.g. until m_offered set size drops to zero.
 * SourceInfo object may co-exist with DownloadInfo object in same
 * Client object.
 */
class SourceInfo : public ClientExtBase {
public:
	/**
	 * Construct new SourceInfo object.
	 *
	 * @param parent     Parent Client object
	 * @param file       The file this client offers
	 */
	SourceInfo(Client *parent, Download *file);

	//! Destructor; disconnects m_sig if connected
	~SourceInfo();

	/**
	 * @name Generic accessors
	 */
	//@{
	uint32_t       getOffCount()    const { return m_offered.size();   }
	Download*      getReqFile()     const { return m_reqFile;          }
	PartMapPtr     getPartMap()     const { return m_partMap;          }
	uint32_t       getQR()          const { return m_qr;               }
	bool           hasNeededParts() const { return m_needParts;        }
	bool           isFullSource()   const { return !m_partMap->size(); }
	uint64_t       getLastSrcExch() const { return m_lastSrcExch;      }

	void setQR(uint32_t qr) { m_qr = qr; }
	void setPartMap(const std::vector<bool> &pm);
	void setReqFile(Download *file);
	void addOffered(Download *file);
	void addFileName(MetaData *md, const std::string &name);
	void swapToLowest(); // swaps reqfile to least available file

	/**
	 * Remove an offered file from this source.
	 *
	 * @param file        File this source is no longer offering
	 * @param cleanUp     If true, also clean up our stuff from PartData
	 */
	void remOffered(Download *file, bool cleanUp = true);

	//! @returns true if *this offers @param file
	bool offers(Download *file) const {
		return m_offered.find(file) != m_offered.end();
	}
	//@}

	//! \returns Number of SourceInfo objects alive
	static size_t count();
private:
	/**
	 * Checks if we need parts from this client, and sets m_needParts
	 * variable accordingly. Assumes m_pm contains the remote client's
	 * offered parts map.
	 */
	void checkNeedParts();

	std::set<Download*> m_offered;  //!< Offered files
	Download*      m_reqFile;       //!< Currently requested file
	PartMapPtr     m_partMap;       //!< Chunks the client has
	uint32_t       m_qr;            //!< Queue rank; Note: QR 0 == QueueFull
	bool           m_needParts;     //!< If we need smth
	uint64_t       m_lastSrcExch;   //!< Last time we did SourceExchange

	//! File name added to MetaData
	std::map<MetaData*, std::string> m_fileNamesAdded;
};

} // namespace Detail
} // end namespace Donkey

#endif
