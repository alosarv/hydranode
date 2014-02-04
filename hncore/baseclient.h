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

#ifndef __BASECLIENT_H__
#define __BASECLIENT_H__

/**
 * \file baseclient.h Interface for BaseClient class
 */

#include <hnbase/osdep.h>
#include <hncore/fwd.h>
#include <boost/scoped_ptr.hpp>
#include <set>

#if 0
class ChunkMask : public std::vector<bool> {
public:
	ChunkMask(uint32_t chunkSize) : m_chunkSize(chunkSize) {}
	uint32_t getChunkSize() const { return m_chunkSize; }
private:
	ChunkMask();
	uint32_t m_chunkSize;
};
#endif

namespace Detail {
	class ClientManagerIterator;
}

/**
 * BaseClient class acts as abstract base class for plugin-defined concrete
 * client classes. It provides user interfaces access to client data through
 * virtual functions. Derived classes can implement one or more of the virtual
 * methods to provide user interfaces with the data.
 *
 * Furthermore, BaseClient is automatically stored in ClientManager, which
 * provides various "views" of the clients listing to user interfaces; in
 * order to fully exploit that, it's strongly recommended that the derived
 * classes use the methods found in the 'modifiers' section of the interface
 * to keep the base class updated about the current state of the client
 * (connected, uploading/downloading, queued and such).
 */
class HNCORE_EXPORT BaseClient {
public:
	/**
	 * Only allowed constructor.
	 *
	 * @param module        Module this client belongs to
	 */
	BaseClient(ModuleBase *module);

	//! Pure virtual destructor
	virtual ~BaseClient() = 0;

	/**
	 * \name Base accessors
	 */
	//!@{
	ModuleBase* getModule()     const { return m_module;        }
	SharedFile* getReqFile()    const { return m_reqFile;       }
	PartData*   getSource()     const { return m_partData;      }
	bool        wantsUpload()   const { return m_reqFile;       }
	bool        canDownload()   const { return m_partData;      }
	bool        isUploading()   const { return m_isUploading;   }
	bool        isDownloading() const { return m_isDownloading; }
	bool        isConnected()   const { return m_isConnected;   }
	//!@}

	/**
	 * \name Virtual accessors
	 */
	//!@{
	virtual IPV4Address getAddr()              const;
	virtual std::string getNick()              const;
	virtual std::string getSoft()              const;
	virtual std::string getSoftVersion()       const;
	virtual uint64_t    getSessionUploaded()   const;
	virtual uint64_t    getSessionDownloaded() const;
	virtual uint64_t    getTotalUploaded()     const;
	virtual uint64_t    getTotalDownloaded()   const;
	virtual uint32_t    getUploadSpeed()       const;
	virtual uint32_t    getDownloadSpeed()     const;
	virtual uint32_t    getQueueRanking()      const;
	virtual uint32_t    getRemoteQR()          const;
	//!@}

	/**
	 * \name Modifiers
	 */
	//!@{
	//! @param file     This client is active source for this file
	void setSource(PartData *file);
	//! @param file     This client is offering this file (also)
	void addOffered(PartData *file);
	/**
	 * @param file      This client no longer offers this file
	 * @param other     If present, switch "active" offering to this
	 */
	void remOffered(PartData *file, PartData *other = 0);
	//! @param file     This client requests this file from us
	void setRequest(SharedFile *file);
	//! @param state    Changes "connected" status
	void setConnected(bool state);
	/**
	 * @param state     Changes "downloading" status
	 * @param file      File we'r downloading from this client
	 */
	void setDownloading(bool state, PartData *file);
	/**
	 * @param state     Changes "uploading" status
	 * @param file      File we'r uploading to this client
	 */
	void setUploading(bool state, SharedFile *file);
	//!@}

	// utilities
	// TODO: This section should contain useful utilities to reduce
	// duplicate code across modules, such as chunk request generation
//	Detail::LockedRangePtr getChunkReq(
//		uint32_t size, const ChunkMask &pred
//	);
private:
	// default construction, as well as copying is not allowed
	BaseClient();
	BaseClient(const BaseClient&);
	BaseClient& operator=(const BaseClient&);

	ModuleBase                  *m_module;   //!< Governing module
	SharedFile                  *m_reqFile;  //!< Requested file
	PartData                    *m_partData; //!< Offered file
	std::set<PartData*>          m_offered;  //!< Additional offered files
//	boost::scoped_ptr<ChunkMask> m_chunkMask;

	bool                 m_isConnected;      //!< Connected status
	bool                 m_isUploading;      //!< Uploading status
	bool                 m_isDownloading;    //!< Downloading status
//	Detail::UsedRangePtr m_usedRange;        //!< Used range (if any)

	//! Used by implementation, iterator for this client in ClientManager
	boost::scoped_ptr<Detail::ClientManagerIterator> m_iter;
};

#endif
