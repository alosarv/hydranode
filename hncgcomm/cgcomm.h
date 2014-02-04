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

#ifndef __CGCOMM_H__
#define __CGCOMM_H__

#include <hncgcomm/osdep.h>
#include <hncgcomm/fwd.h>
#include <hncore/cgcomm/opcodes.h>
#include <boost/function.hpp>
#include <boost/signal.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/utility.hpp>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace Engine {
	extern boost::signal<void (const std::string&)> debugMsg;

	namespace Detail {
		struct MainSubMap;
	}

	// Main class initializes core/gui communication, and provides front-end
	// for inputting data to the parser, as well as signals when user should
	// send specific data back to the engine over the socket.
	//
	// All other subsystem constructors require pointer to this class as
	// first construction argument, since they need to know information
	// found here.
	class DLLEXPORT Main {
	public:
		// Function prototype used for outgoing data
		typedef boost::function<void (const std::string&)> SendFunc;

		// Main constructor; requires a function to be used for outgoing
		// data
		Main(SendFunc func);

		~Main();

		// Parse data received from engine
		void parse(const std::string &data);

		void addSubSys(SubSysBase *sys);
		void delSubSys(SubSysBase *sys);

		// Sends request to shut down engine completely
		void shutdownEngine();
	private:
		// default constructor is forbidden
		Main();

		// copying is forbidden
		Main(const Main&);
		Main& operator=(const Main&);

		friend class SubSysBase; // needs to access sendData

		SendFunc sendData;       // sends data to engine
		std::string m_buffer;    // input buffer

		// list of subsystems active at this time
		Detail::MainSubMap *m_list;
	};

	class DLLEXPORT SubSysBase : boost::noncopyable {
	public:
		SubSysBase(Main *parent, uint8_t subCode);
		virtual ~SubSysBase();
		virtual void handle(std::istream &packet) = 0;
		uint8_t getSubCode() const;
	protected:
		void sendPacket(const std::string &data);
	private:
		SubSysBase();

		Main *m_parent;
		uint8_t m_subCode;
	};

	/////////////////////////
	// Searching subsystem //
	/////////////////////////

	class DLLEXPORT SearchResult : boost::noncopyable {
	public:
		SearchResult(Search *parent);

		std::string getName()        const { return m_name;        }
		uint64_t    getSize()        const { return m_size;        }
		uint32_t    getSources()     const { return m_sources;     }
		uint32_t    getFullSources() const { return m_fullSources; }

		uint32_t    getBitrate() const { return m_bitrate; }
		uint32_t    getLength()  const { return m_length;  }
		std::string getCodec()   const { return m_codec;   }

		void download();
		void download(const std::string &dest);
		boost::signal<void()> onUpdated;
	private:
		SearchResult();
		void update(SearchResultPtr res);

		friend class Search;
		Search*     m_parent; //!< Needed for download() method
		std::string m_name;
		uint64_t    m_size;
		uint32_t    m_sources;
		uint32_t    m_fullSources;
		uint32_t    m_num;
		// multimedia-specific
		uint32_t    m_bitrate;
		uint32_t    m_length;
		std::string m_codec;
	};

	// provides means for performing searches
	class DLLEXPORT Search : public SubSysBase {
	public:
		typedef boost::function<
			void (const std::vector<SearchResultPtr>&)
		> ResultHandler;

 		Search(
			Main *parent, ResultHandler handler,
			const std::string &keywords = "",
			FileType ft = FT_UNKNOWN
		);

		// run the search
		void run();

		void addKeywords(const std::string &keywords);
		void setType(FileType ft);
		void setMinSize(uint64_t size);
		void setMaxSize(uint64_t size);

	protected:
		// handle data coming from engine
		virtual void handle(std::istream &packet);
	private:
		Search();

		friend class SearchResult;

		// signal that will be emitted when new results are found
		ResultHandler m_sigResults;
		std::string   m_keywords;
		uint64_t      m_minSize;
		uint64_t      m_maxSize;
		FileType      m_fileType;
		uint32_t      m_lastNum;
		std::map<uint32_t, SearchResultPtr> m_results;
	};

	///////////////////////////
	// Downloading Subsystem //
	///////////////////////////

	typedef boost::shared_ptr<DownloadInfo> DownloadInfoPtr;

	enum DownloadState {
		DSTATE_RUNNING   = ::CGComm::DSTATE_RUNNING,
		DSTATE_VERIFYING = ::CGComm::DSTATE_VERIFYING,
		DSTATE_MOVING    = ::CGComm::DSTATE_MOVING,
		DSTATE_COMPLETED = ::CGComm::DSTATE_COMPLETE,
		DSTATE_CANCELED  = ::CGComm::DSTATE_CANCELED,
		DSTATE_PAUSED    = ::CGComm::DSTATE_PAUSED,
		DSTATE_STOPPED   = ::CGComm::DSTATE_STOPPED
	};

	class DLLEXPORT DownloadInfo
		: public boost::enable_shared_from_this<DownloadInfo> {
	public:
		typedef std::set<DownloadInfoPtr>::const_iterator CIter;

		CIter begin()      const { return m_children.begin(); }
		CIter end()        const { return m_children.end();   }
		bool hasChildren() const { return m_children.size();  }
		bool hasChild(DownloadInfoPtr p) const {
			return m_children.find(p) != m_children.end();
		}

		~DownloadInfo();

		void pause();
		void stop();
		void resume();
		void cancel();
		void setName(const std::string &newName);
		void setDest(const std::string &newDest);

		std::string   getName()       const { return m_name;       }
		uint64_t      getSize()       const { return m_size;       }
		uint64_t      getCompleted()  const { return m_completed;  }
		uint32_t      getSourceCnt()  const { return m_srcCnt;     }
		uint32_t      getFullSrcCnt() const { return m_fullSrcCnt; }
		std::string   getDestDir()    const { return m_destDir;    }
		uint32_t      getId()         const { return m_id;         }
		uint32_t      getSpeed()      const { return m_speed;      }
		DownloadState getState()      const { return m_state;      }
		std::string   getLocation()   const { return m_location;   }
		uint8_t       getAvail()      const { return m_avail;      }

		void getNames();
		void getComments();
		void getLinks();

		typedef std::set<std::string>::const_iterator CommentIter;
		typedef std::set<std::string>::const_iterator LinkIter;
		typedef std::map<std::string,uint32_t>::const_iterator NameIter;

		CommentIter cbegin() const { return m_comments.begin(); }
		CommentIter cend()   const { return m_comments.end();   }
		size_t      ccount() const { return m_comments.size();  }
		LinkIter    lbegin() const { return m_links.begin();    }
		LinkIter    lend()   const { return m_links.end();      }
		size_t      lcount() const { return m_links.size();     }
		NameIter    nbegin() const { return m_names.begin();    }
		NameIter    nend()   const { return m_names.end();      }
		size_t      ncount() const { return m_names.size();     }

		boost::signal<void ()> onUpdated;
		boost::signal<void ()> onDeleted;
	private:
		friend class DownloadList;

		DownloadInfo(DownloadList *parent, uint32_t id);
		DownloadInfo(const DownloadInfo&);

		void update(const DownloadInfo &o);

		std::string   m_name;
		uint64_t      m_size;
		uint64_t      m_completed;
		std::string   m_destDir;
		uint32_t      m_srcCnt;
		uint32_t      m_fullSrcCnt;
		uint32_t      m_speed;
		DownloadState m_state;
		std::string   m_location;
		uint8_t       m_avail;
		uint32_t      m_id;

		// these three are not sent automatically, but must be requested
		// explicitly via getComments(), getNames() and getLinks()
		// methods. onUpdated() signal is emitted when the data arrives.
		std::set<std::string> m_comments;
		std::map<std::string, uint32_t> m_names;
		std::set<std::string> m_links;

		std::set<DownloadInfoPtr> m_children;
		DownloadList *m_parent;
	};

	// access the download list
	class DLLEXPORT DownloadList : public SubSysBase {
	public:
		DownloadList(Main *parent);

		// Request the list of current downloads.
		void getList();

		// monitor the list, receiving updates whenever anything changes
		void monitor(uint32_t interval);

		void pause(DownloadInfoPtr d);
		void stop(DownloadInfoPtr d);
		void resume(DownloadInfoPtr d);
		void cancel(DownloadInfoPtr d);
		void getNames(DownloadInfoPtr d);
		void getComments(DownloadInfoPtr d);
		void getLinks(DownloadInfoPtr d);
		void setName(DownloadInfoPtr d, const std::string &newName);
		void setDest(DownloadInfoPtr d, const std::string &newDest);

		void downloadFromLink(const std::string &link);
		void downloadFromFile(const std::string &contents);
		void importDownloads(const std::string &dir);
	
		boost::signal<void (DownloadInfoPtr)> onAdded;
		boost::signal<void (DownloadInfoPtr)> onRemoved;
		boost::signal<void (DownloadInfoPtr)> onUpdated;
		boost::signal<void (DownloadInfoPtr)> onUpdatedNames;
		boost::signal<void (DownloadInfoPtr)> onUpdatedComments;
		boost::signal<void (DownloadInfoPtr)> onUpdatedLinks;
		boost::signal<void ()> onAddedList;
		boost::signal<void ()> onUpdatedList;
	protected:
		// handle data coming from engine
		virtual void handle(std::istream &packet);
	private:
		DownloadList();

		//! helper function, reads one DownloadInfo from stream
		DownloadInfoPtr readDownload(std::istream &i);
		void foundNames(std::istream &packet);
		void foundComments(std::istream &packet);
		void foundLinks(std::istream &packet);
		void sendRequest(uint8_t oc, uint32_t id);
		std::map<uint32_t, DownloadInfoPtr> m_list;
		typedef std::map<uint32_t, DownloadInfoPtr>::iterator Iter;
	};


	///////////////////////////
	// Shared Files listing  //
	///////////////////////////
	class DLLEXPORT SharedFile 
	: public boost::enable_shared_from_this<SharedFile> {
	public:
		typedef std::set<SharedFilePtr>::const_iterator CIter;

		CIter begin()      const { return m_children.begin(); }
		CIter end()        const { return m_children.end();   }
		bool hasChildren() const { return m_children.size();  }
		bool hasChild(SharedFilePtr p) const {
			return m_children.find(p) != m_children.end();
		}

		std::string getName()       const { return m_name;       }
		std::string getLocation()   const { return m_location;   }
		uint64_t    getSize()       const { return m_size;       }
		uint64_t    getUploaded()   const { return m_uploaded;   }
		uint32_t    getSpeed()      const { return m_speed;      }
		uint32_t    getId()         const { return m_id;         }
		uint32_t    getPartDataId() const { return m_partDataId; }

		boost::signal<void ()> onUpdated;
		boost::signal<void ()> onDeleted;

		~SharedFile();
	private:
		friend class SharedFilesList;

		SharedFile();
		SharedFile(const SharedFile&);
		SharedFile(SharedFilesList *parent, uint32_t id);
		SharedFile& operator=(const SharedFile&);

		void update(const SharedFile &o);

		std::string m_name;
		std::string m_location;
		uint64_t    m_size;
		uint64_t    m_uploaded;
		uint32_t    m_speed;
		uint32_t    m_id;
		uint32_t    m_partDataId;

		SharedFilesList *m_parent;
		std::set<SharedFilePtr> m_children;
	};

	class DLLEXPORT SharedFilesList : public SubSysBase {
	public:
		SharedFilesList(Main *parent);

		void getList();
		void monitor(uint32_t interval);
		void addShared(const std::string &dir);
		void remShared(const std::string &dir);

		boost::signal<void (SharedFilePtr)> onAdded;
		boost::signal<void (SharedFilePtr)> onRemoved;
		boost::signal<void (SharedFilePtr)> onUpdated;
		boost::signal<void ()> onAddedList;
		boost::signal<void ()> onUpdatedList;
	protected:
		// handle data coming from engine
		virtual void handle(std::istream &packet);
	private:
		SharedFilesList();

		SharedFilePtr readFile(std::istream &i);
		std::map<uint32_t, SharedFilePtr> m_list;
		typedef std::map<uint32_t, SharedFilePtr>::iterator Iter;
	};

	/////////////////////////////
	// Configuration subsystem //
	/////////////////////////////

	class DLLEXPORT Config : public SubSysBase {
	public:
		typedef boost::function<
			void (const std::map<std::string, std::string>&)
		> ListHandler;

		Config(Main *parent, ListHandler handler);

		void getValue(const std::string &key);
		void setValue(const std::string &key, const std::string &value);
		void getList();
		void monitor();
	protected:
		// handle data coming from engine
		virtual void handle(std::istream &packet);
	private:
		ListHandler m_handler;
		std::map<std::string, std::string> m_list;
	};

	///////////////////////
	// Network subsystem //
	///////////////////////
	
	class DLLEXPORT Network : public SubSysBase {
	public:
		typedef boost::function<void()> UpdateHandler;

		Network(Main *parent, UpdateHandler handler);
		void getList();
		void monitor(uint32_t interval);

		uint32_t getUpSpeed()        const { return m_upSpeed;       }
		uint32_t getDownSpeed()      const { return m_downSpeed;     }
		uint32_t getConnCnt()        const { return m_connCnt;       }
		uint32_t getConnectingCnt()  const { return m_connectingCnt; }
		uint64_t getTotalUp()        const { return m_totalUp;       }
		uint64_t getTotalDown()      const { return m_totalDown;     }
		uint64_t getSessionUp()      const { return m_sessionUp;     }
		uint64_t getSessionDown()    const { return m_sessionDown;   }
		uint64_t getUpPackets()      const { return m_upPackets;     }
		uint64_t getDownPackets()    const { return m_downPackets;   }
		uint32_t getUpLimit()        const { return m_upLimit;       }
		uint32_t getDownLimit()      const { return m_downLimit;     }
		uint64_t getSessionLength()  const { return m_sessLength;    }
		uint64_t getOverallRuntime() const { return m_totalRuntime;  }
	protected:
		virtual void handle(std::istream &i);
	private:
		uint32_t m_upSpeed,   m_downSpeed, m_connCnt,   m_connectingCnt;
		uint64_t m_totalUp,   m_totalDown, m_sessionUp, m_sessionDown;
		uint64_t m_upPackets, m_downPackets;
		uint32_t m_upLimit,   m_downLimit;
		uint64_t m_sessLength,m_totalRuntime;
		UpdateHandler m_handler;
	};

	///////////////////////
	// Modules subsystem //
	///////////////////////

	class DLLEXPORT Module {
	public:
		std::string getName()           const { return m_name;      }
		std::string getDesc()           const { return m_desc;      }
		uint64_t getSessionUploaded()   const { return m_sessUp;    }
		uint64_t getSessionDownloaded() const { return m_sessDown;  }
		uint64_t getTotalUploaded()     const { return m_totalUp;   }
		uint64_t getTotalDownloaded()   const { return m_totalDown; }
		uint32_t getUpSpeed()           const { return m_upSpeed;   }
		uint32_t getDownSpeed()         const { return m_downSpeed; }
		uint32_t getId()                const { return m_id;        }

		friend class Modules;
	private:
		std::string m_name, m_desc;
		uint64_t m_sessUp, m_sessDown, m_totalUp, m_totalDown;
		uint32_t m_upSpeed, m_downSpeed, m_id;
	
		void update(ModulePtr mod);

		Module();
		Module(const Module&);
	};

	class DLLEXPORT Object : public boost::enable_shared_from_this<Object> {
	public:
		typedef std::vector<std::string>::const_iterator DIter;
		typedef std::map<uint32_t, ObjectPtr>::const_iterator CIter;

		std::string getName()   const { return m_name;   }
		uint32_t    getId()     const { return m_id;     }
		ObjectPtr   getParent() const { return m_parent; }

		size_t      dataCount()       const { return m_data.size();  }
		std::string getData(size_t n) const { return m_data.at(n);   }
		DIter       dbegin()          const { return m_data.begin(); }
		DIter       dend()            const { return m_data.end();   }

		size_t childCount() const { return m_children.size();  }
		CIter  begin()      const { return m_children.begin(); }
		CIter  end()        const { return m_children.end();   }

		void doOper(
			const std::string &opName,
			const std::map<std::string, std::string> &args
		);
		void setData(uint8_t num, const std::string &newValue);

		boost::signal<void (ObjectPtr, ObjectPtr)> childAdded;
		boost::signal<void (ObjectPtr, ObjectPtr)> childRemoved;
		boost::signal<void (ObjectPtr)> onDestroyed;
	private:
		friend class Modules;

		ObjectPtr m_parent;
		Modules *m_parentList;
		std::vector<std::string> m_data;
		std::map<uint32_t, ObjectPtr> m_children;
		std::set<uint32_t> m_childIds;
		std::string m_name;
		uint32_t m_id;

		void update(ObjectPtr obj);
		void findChildren();
		void destroy();

		Object(Modules *parent);
		Object(const Object&);
		Object& operator=(const Object&);
	};

	class DLLEXPORT Modules : public SubSysBase {
	public:
		Modules(Main *parent);

		// modules specific
		void getList();
		void monitor(uint32_t interval);

		boost::signal<void ()> onUpdated;
		boost::signal<void (ModulePtr)> onAdded;
		boost::signal<void (ModulePtr)> onRemoved;

		typedef std::map<uint32_t, ModulePtr>::iterator Iter;
		typedef std::map<uint32_t, ModulePtr>::const_iterator CIter;

		size_t count() const { return m_list.size();  }
		CIter  begin() const { return m_list.begin(); }
		CIter  end()   const { return m_list.end();   }

		// Object tree specific

		/**
		 * Request an object to be sent
		 *
		 * @param mod          Module for which the object is child of
		 * @param name         Name of the object to be sent
		 * @param recurse      Whether to send the entire subtree
		 * @param monitorTimer If non-zero, also setup monitoring (ms)
		 */
		void getObject(
			ModulePtr mod, const std::string &name, 
			bool recurse = true, uint32_t monitorTimer = 0
		);
		/**
		 * Requests monitoring to be set up for the object
		 *
		 * @param obj       Object to be monitored
		 * @param interval  Minimum update interval (ms)
		 */
		void monitorObject(ObjectPtr obj, uint32_t interval);

		void doObjectOper(
			ObjectPtr obj, const std::string &opName,
			const std::map<std::string, std::string> &args
		);
		void setObjectData(
			ObjectPtr obj, uint8_t dNum, const std::string &newValue
		);

		typedef std::map<uint32_t, ObjectPtr>::iterator OIter;
		typedef std::map<uint32_t, ObjectPtr>::const_iterator OCIter;

		ObjectPtr findObject(uint32_t id) const {
			OCIter i = m_objects.find(id);
			if (i != m_objects.end()) {
				return i->second;
			} else {
				return ObjectPtr();
			}
		}

		/**
		 * Emitted when an object is received from core. Note that when
		 * receiving multiple objects in a tree, this is emitted only
		 * for the top-most tree object, instead of for each subobject
		 * of the tree.
		 */
		boost::signal<void (ObjectPtr)> receivedObject;

		/**
		 * Signaled when an object has been updated. This is emitted
		 * for each object that was updated.
		 */
		boost::signal<void (ObjectPtr)> updatedObject;

		boost::signal<void (ObjectPtr)> addedObject;
		boost::signal<void (ObjectPtr)> removedObject;
	protected:
		virtual void handle(std::istream &i);
	private:
		std::map<uint32_t, ModulePtr> m_list;

		std::map<uint32_t, ObjectPtr> m_objects;

		ModulePtr readModule(std::istream &i);
		ObjectPtr readObject(std::istream &i);
	};

} // end namespace Engine

#endif
