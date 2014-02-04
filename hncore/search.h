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
 * \file search.h Interface for Searching API
 */

#ifndef __SEARCH_H__
#define __SEARCH_H__

#include <hnbase/osdep.h>
#include <hncore/fwd.h>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/signal.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <hnbase/ipv4addr.h>

/**
 * Search result encapsualtes a single result from a search. It is-a MetaData,
 * and inherits all the properties of MetaData object. This provides maximum
 * extedability in search results, since they can contain ALL kinds of meta
 * data supported by the MetaData and member structures.
 */
class HNCORE_EXPORT SearchResult {
public:
	typedef std::set<std::string>::const_iterator CNIter;
	typedef std::set<IPV4Address>::const_iterator CSIter;

	SearchResult(const std::string &name, uint64_t fileSize);
	virtual ~SearchResult();

	/**
	 * Download this search result. Must be overridden by derived classes,
	 * e.g. the module providing this search result, since only that module
	 * can safely start the download.
	 */
	virtual void download() = 0;

	/**
	 * @returns Globally unique identifier for this search result.
	 */
	virtual std::string identifier() const = 0;

	//! @name Accessors
	//@{
	void addSources(uint32_t amount)  { m_sourceCnt += amount;   }
	void addComplete(uint32_t amount) { m_completeCnt += amount; }
	void addRating(uint8_t rating)    { m_rating = rating;       }
	void addName(const std::string &n){ m_names.insert(n);       }
	void addSource(IPV4Address s)     { m_sources.insert(s);     }
	uint32_t    getSources()    const { return m_sourceCnt;      }
	uint32_t    getComplete()   const { return m_completeCnt;    }
	std::string getName()       const { return m_fileName;       }
	uint64_t    getSize()       const { return m_fileSize;       }
	uint8_t     getRating()     const { return m_rating;         }
	boost::shared_ptr<AudioMetaData>&   getAmd()  { return m_amd;  }
	boost::shared_ptr<VideoMetaData>&   getVmd()  { return m_vmd;  }
	boost::shared_ptr<ArchiveMetaData>& getArmd() { return m_armd; }
	boost::shared_ptr<ImageMetaData>&   getImd()  { return m_imd;  }
	boost::shared_ptr<StreamData>&      getStrd() { return m_strd; }
	CNIter nbegin() const { return m_names.begin();   }
	CNIter nend()   const { return m_names.end();     }
	CSIter sbegin() const { return m_sources.begin(); }
	CSIter send()   const { return m_sources.end();   }
	//@}
private:
	SearchResult();                                   //!< Forbidden
	SearchResult(const SearchResult&);                //!< Forbidden
	SearchResult& operator=(const SearchResult&);     //!< Forbidden

	std::string m_fileName;          //!< Name of the file
	uint64_t    m_fileSize;          //!< Size of the file
	uint32_t    m_sourceCnt;         //!< Number of sources
	uint32_t    m_completeCnt;       //!< Number of complete sources
	uint8_t     m_rating;            //!< File rating
	std::set<std::string> m_names;   //!< Extra file names
	std::set<IPV4Address> m_sources; //!< Found sources

	//! @name Various MetaData received from search results
	//@{
	boost::shared_ptr<AudioMetaData>   m_amd;
	boost::shared_ptr<VideoMetaData>   m_vmd;
	boost::shared_ptr<ArchiveMetaData> m_armd;
	boost::shared_ptr<ImageMetaData>   m_imd;
	boost::shared_ptr<StreamData>      m_strd;
	//@}
};

/**
 * Search class represents one search sent to one or more modules for
 * processing, emitting events when new results are added, which the original
 * request creator can then handle.
 *
 * The basic usage of this object evolves around constructing the object on
 * heap, but within boost::shared_ptr, then customizing the search by
 * adding terms and setting other limits, and then passing the pointer to
 * Search::run() static function, which then sends the object to all search
 * handlers.
 *
 * Modules wishing to support searching may register their search handler
 * method using addHandler() member function (Note: EventTable class also
 * defines a function with same name, however that one is accessible through
 * getEventTable() member function, as defined by Event Handling API). When
 * new search is performed, all handler functions are notified about the search,
 * and can thus perform module-dependant searching procedure, adding results
 * and posting events.
 *
 * Module developers: When you are adding many results at once to a Search
 * object, do not post a new event after each addition. Instead, post event
 * after all results that are available to that moment have been added to this
 * object. This is needed to save event's processing overhead, as well as
 * processing overhead at target search client.
 */
class HNCORE_EXPORT Search : public boost::enable_shared_from_this<Search> {
public:
	/**
	 * Construct empty search. The object should later be customized by
	 * by adding terms and limitations to the search, before run()'ing it.
	 */
	Search();

	/**
	 * Construct a new search object. This should  be called by client code
	 * which wishes to perform the search.
	 *
	 * @param terms         Search terms, separated by spaces
	 */
	Search(const std::string &terms);

	/**
	 * Execute the search. Call this method after setting up event handlers
	 * for results.
	 *
	 * @param search           Search to be run.
	 */
	void run();

	/**
	 * Stops searching.
	 */
	void stop();

	/**
	 * Add a new search. This is used by search handlers to append new
	 * search results to the object.
	 *
	 * @param result           Search result to be added.
	 */
	void addResult(SearchResultPtr result);

	/**
	 * Add a search query handler, which shall be called whenever someone
	 * runs a new search. It is recommended that the object is set up
	 * using boost::connection::trackable to automate the removal process.
	 */
	static void addQueryHandler(SearchHandler handler);

	/**
	 * Set up a results handler, which shall be notified whenever someone
	 * adds new results to this search. Note that the actual notification
	 * is done on demand by the results adder via notifyResults() method,
	 * not whenever a new result is added to the query. This is done to
	 * avoid excessive function calls when adding many results at same time.
	 */
	template<typename T>
	boost::signals::connection addResultHandler(T handler) {
		return m_sigResult.connect(handler);
	}

	/**
	 * Add a handler for links to be downloaded. A link is defined as any
	 * string. It may be a ed2k://, http://, ftp:// links, but it may also
	 * be a full path to a local file for example - it's up to the modules
	 * to decide what they can (or cannot) accept as "download starter"
	 * string.
	 */
	static void addLinkHandler(LinkHandler handler);

	/**
	 * Add a handler for files to read downloading info from. A file can
	 * contain a list of files to be downloaded. Examples of files are
	 * .torrent's, .emulecollection's and so on. The entire contents of the
	 * file is passed to the handler, rather than the physical path of the
	 * file, so that this function could be used to remotely upload a file
	 * to hydranode.
	 */
	static void addFileHandler(FileHandler handler);

	/**
	 * Start a direct download from link. The passed here is sent to all
	 * link handlers. The handler handling the download will return true,
	 * others return false.
	 *
	 * @param link link to be downloaded; can be path to a file as well
	 * @returns    true if a handler accepted the link; false otherwise
	 */
	static bool downloadLink(const std::string &link);

	/**
	 * Start downloading from a file. See addFileHandler() method for more
	 * details on this functionality.
	 *
	 * @param contents   entire contents of the file to be downloaded
	 * @returns          true if handler accepted the file; false otherwise
	 */
	static bool downloadFile(const std::string &contents);

	/**
	 * Notify all results handlers that there are new results added to this
	 * search query.
	 */
	void notifyResults();

	//! @name Accessors for various internal structures
	//@{
	uint32_t    getTermCount()           const { return m_terms.size();    }
	std::string getTerm(uint32_t num)    const { return m_terms.at(num);   }
	uint32_t    getResultCount()         const { return m_results.size();  }
	uint64_t    getMinSize()             const { return m_minSize;         }
	uint64_t    getMaxSize()             const { return m_maxSize;         }
	FileType    getType()                const { return m_type;            }
	bool        isRunning()              const { return m_running;         }
	SearchResultPtr getResult(uint32_t num) const;
	SearchResultPtr getResult(const std::string &id) const;
	//@}

	//! @name Setters
	//@{
	void addTerm(const std::string &term)       { m_terms.push_back(term); }
	void setMinSize(uint64_t size)                  { m_minSize = size;    }
	void setMaxSize(uint64_t size)                  { m_maxSize = size;    }
	void setType(FileType type)                     { m_type = type;       }
	//@}

	//! constant iterator to underlying data container
	typedef std::vector<SearchResultPtr>::const_iterator CIter;

	//! @returns iterator to the beginning of results vector
	CIter begin() const { return m_results.begin(); }
	//! @returns iterator to one-past-end of results container
	CIter end() const { return m_results.end(); }
private:
	// Internal data
	// -------------

	//! Used for sorting m_resultsById by unique string identifier
	struct IDExtractor {
		typedef std::string result_type;
		result_type operator()(const SearchResultPtr &r) const {
			return r->identifier();
		}
	};

	typedef boost::multi_index_container<
		SearchResultPtr,
		boost::multi_index::indexed_by<
			boost::multi_index::hashed_unique<IDExtractor>
		>
	> IDContainer;

	//! Results, random-accessible
	std::vector<SearchResultPtr> m_results;
	//! Results, searchable
	IDContainer m_resultsById;

	std::vector<std::string> m_terms;         //!< Terms to search for
	uint64_t m_minSize;                       //!< Minimum file size
	uint64_t m_maxSize;                       //!< Maximum file size
	FileType m_type;                          //!< File type
	bool     m_running;                       //!< If the search is active

	// Notification mechanisms
	// -----------------------

	/**
	 * This signal is fired when new results are added to an existing
	 * search. This is fired by Search object in addResult method.
	 */
	boost::signal<void (SearchPtr)> m_sigResult;

	/**
	 * This signal is fired whenever a new search query is started. Modules
	 * must connect their handlers to this signal to receive search query
	 * notifications.
	 */
	static boost::signal<void (SearchPtr)> s_sigQuery;

	/**
	 * Handlers for text-based link downloadings.
	 */
	static std::vector<LinkHandler> s_linkHandlers;

	/**
	 * Handlers for file-based downloadings.
	 */
	static std::vector<FileHandler> s_fileHandlers;
};

#endif
