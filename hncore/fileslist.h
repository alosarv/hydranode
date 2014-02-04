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
 * \file fileslist.h Interface for FilesList class
 */

#ifndef __FILESLIST_H__
#define __FILESLIST_H__

/**
 * \page fms Files Management Subsystem Overview
 *
 * The three main classes in Files Management Subsystem are PartData,
 * SharedFile and FilesList. They are supported by MetaData data structures,
 * which contain useful information about these objects. PartData object
 * is owned by SharedFile, which in turn is owned by FilesList. Various
 * ***MetaData objects are owned by MetaData, which in turn is owned by
 * MetaDb. The usage of the subsystem goes as follows:
 *
 * \section newdl Starting a new download.
 * - The plugin which receives a request to start a new download creates a new
 *   PartData object. The plugin must pass the size of the new file as well as
 *   the expected (final) file name of the download. A temporary file will be
 *   then created in one of the temp directories.
 * - If the plugin has more information about the file, it can construct the
 *   neccesery MetaData objects, insert the data into those and push the
 *   MetaData into MetaDb.
 * - Finalizing the download creation, the plugin requests FilesList to
 *   construct a new download via the method createDownload(), which takes care
 *   of the remaining details.
 *
 * Example:
 * \code
 * SharedFile *sf = MetaDb::instance().findSharedFile(hash);
 * if (sf && sf->isPartial()) {
 *   logMsg(
 *     boost::format("You are already attempting to download %s")
 *     % sf->getName()
 *   );
 * } else if (sf) {
 *   logMsg(boost::format("You already have %s") % sf->getName());
 * } else {
 *   MetaData *md = new MetaData(size);
 *   md->addFileName(name);
 *   md->addHashSet(myhashset);
 *   MetaDb::instance().push(md);
 *   FilesList::instance().createDownload(name, md);
 * }
 * \endcode
 *
 * \section olddl Continueing an existing download.
 * When the application restarts, the plugins want to continue downloading
 * existing partial files. In order to do so, plugins must locate all the files
 * they are capable of downloading from FilesList. Here's the hard part. We
 * don't want to force plugins to keep internal list of their part files -
 * after all, its (a) our job, (b) de-centralizes this stuff, (c) its our job.
 * So, we need to provide the plugins with a list of files they can attempt to
 * download. Consider this example: We have been downloading files from various
 * networks for quite some time now, having large amount of partial files. Now,
 * a new plugin for a new network gets released. Naturally, the new plugin
 * joins the show and wants to catch up to things - e.g. what needs to be done.
 * So the plugin does what? Takes up FilesList and iterates on it. Only the
 * plugin itself can know what kind of files its looking for - for example,
 * some plugins might need a specific hash in order to download a file, others
 * might want http or ftp url ... god knows what. So - in short - FilesList
 * provides access to begin() and end() iterators, just as any standard c++
 * container, and let plugins do the hard part.
 *
 * Ok - enough discussion - so what do you do to get your files ? Call
 * FilesList::begin() to retrieve iterator to begin of list, and start
 * iterating on the list. The only guarantee I can give you at this point is
 * that if you dereference the iterator, you get a pointer to a SharedFile
 * object. You don't even know that its partial - use the SharedFile member
 * functions to check it. Where we go from here is up to you to decide. The
 * iterators are constant, so you can't modify them. And don't store them -
 * they are std::map iterators, and will become invalid when the integrity of
 * the map changes.
 */

#include <hnbase/osdep.h>
#include <hnbase/object.h>
#include <hncore/fwd.h>
#include <hncore/sharedfile.h>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <string>
#include <set>
#include <iosfwd>

/**
 * FilesList class owns all SharedFile objects and provides accessors for
 * managing shared files / directories as well as temporary files /
 * directories.
 */
class HNCORE_EXPORT FilesList : public Object {
public:
	struct PathExtractor {
		typedef boost::filesystem::path result_type;
		result_type operator()(const SharedFile *const s) const {
			return s->getPath();
		}
	};
	struct PartExtractor {
		typedef bool result_type;
		result_type operator()(const SharedFile *const s) const {
			return s->isPartial();
		}
	};
	typedef boost::multi_index_container<
		SharedFile*,
		boost::multi_index::indexed_by<
			boost::multi_index::ordered_unique<
				boost::multi_index::identity<SharedFile*>
			>,
			boost::multi_index::ordered_unique<PathExtractor>,
			boost::multi_index::ordered_non_unique<PartExtractor>
		>
	> MIFilesList;
	typedef MIFilesList::nth_index<0>::type::iterator SFIter;
	typedef MIFilesList::nth_index<0>::type::const_iterator CSFIter;
	typedef MIFilesList::nth_index<1>::type::iterator NIter;
	typedef MIFilesList::nth_index<1>::type::const_iterator CNIter;
	typedef MIFilesList::nth_index<2>::type::iterator PDIter;
	typedef MIFilesList::nth_index<2>::type::iterator CPDIter;

	/**
	 * Access to the single instance of this class
	 */
	static FilesList& instance();

	/**
	 * \brief Add new SharedFile to this list
	 *
	 * @param file     File to be added
	 */
	void push(SharedFile *file);

	/**
	 * \brief Scan a folder for shared files
	 *
	 * @param path     Path to folder to be scanned
	 * @param recurse  If true, the path will be scanned recursivly
	 */
	void addSharedDir(const std::string &path, bool recurse = false);

	/**
	 * \brief Scan a temporary folder for temp files
	 *
	 * @param p        Path to the folder to be added
	 * @param scan     If true, the path will be scanned now
	 */
	void addTempDir(const std::string &path, bool scan = true);

	/**
	 * \brief Remove a shared folder, un-sharing all files found in there
	 *
	 * @param path     Path to be un-shared
	 * @param recurse  If all sub-dirs should be un-shared too
	 */
	void remSharedDir(const std::string &path, bool recurse = false);

	/**
	 * \brief Remove a temporary folder.
	 *
	 * @param path     Path to be removed
	 *
	 * The files in the temporary folder will not be modified; existing
	 * downloads will be saved and then destroyed, the data on disk is
	 * kept, so the path can be re-added later on to resume those downloads.
	 */
	void remTempDir(const std::string &path);

	/**
	 * Add a new download, constructed from the passed MetaData info.
	 *
	 * @param name      Name of the expected resulting file
	 * @param info      Information about the new download
	 * @param dest      Optional destination location (excluding filename)
	 *
	 * This function doesn't return the newly created download on purpose -
	 * namely, this (or rather - the lack of this) enforces a number of
	 * things on plugin design, for example it instantly requires the plugin
	 * to implement downloads resuming, as well as warns about several bad
	 * design practices. As such, this function should never be changed to
	 * return the object created.
	 *
	 * If absolutely neccesery to find the file created right after calling
	 * this function, you can look it up in MetaDb using the info parameter.
	 */
	void createDownload(
		std::string name, MetaData *info,
		boost::filesystem::path dest = boost::filesystem::path()
	);

	/**
	 * This signal is emitted when createDownload successfully starts a
	 * download.
	 */
	boost::signal<void (PartData*)> onDownloadCreated;

	/**
	 * @name Generic accessors
	 */
	//@{
	size_t  size()  const { return m_list.size();  }
	CSFIter begin() const { return m_list.begin(); }
	CSFIter end()   const { return m_list.end();   }
	std::pair<CNIter, CNIter> equal_range(const std::string &name) const {
		return m_list.get<1>().equal_range(name);
	}
	template<size_t N>
	typename MIFilesList::nth_index<N>::type::const_iterator end() {
		return boost::multi_index::get<N>(m_list).end();
	}
	std::pair<PDIter, PDIter> getTempFiles() const {
		return m_list.get<2>().equal_range(true);
	}
	//@}

	/**
	 * Instructs FilesList to save the state of all SharedFiles which
	 * have PartData member.
	 *
	 * @param saveAll     Whether to save all files, including those that
	 *                    don't have anything in buffers.
	 */
	void savePartFiles(bool saveAll = false);

	//! Returns number of files with PartData member
	uint32_t getPartialCount() const {
		return m_list.get<2>().count(true);
	}

	//! Returns number of total entries in this list; equal to size()
	uint32_t getSharedCount() const { return size(); }

	//! Attempt to import files from a location
	boost::signal<void (boost::filesystem::path)> import;

	//! Saves known shared/temp dirs to settings
	void saveSettings() const;
private:
	void scanSharedDir(const std::string &path, bool recurse = false);
	void scanTempDir(const std::string &path);

	friend class Hydranode;
	void exit(); //! Performs shutdown cleanup

	//! @name Singleton class - constructors/destructors hidden
	//@{
	FilesList();
	~FilesList();
	FilesList(FilesList &);
	FilesList operator=(const FilesList&);
	//@}

	//! Main list
	MIFilesList m_list;

	//! Map of shared directories. boolean indicates recursion
	std::map<std::string, bool> m_sharedDirs;
	typedef std::map<std::string, bool>::iterator SDIter;

	//! Temporary directories
	std::set<std::string> m_tempDirs;
	typedef std::set<std::string>::iterator TDIter;

	//! Output operator to streams
	friend std::ostream& operator<<(std::ostream &o, const FilesList &fl);

	/**
	 * Verify that the string contains a valid existing path.
	 *
	 * @param path   Path to verify.
	 *
	 * \throws std::runtime_error if verification fails.
	 */
	static void verifyPath(const std::string &path);

	/**
	 * Event handler for SharedFile events
	 *
	 * @param sf      SharedFile triggering the event.
	 * @param evt     Event type.
	 */
	void onSharedFileEvent(SharedFile *sf, int evt);

	/**
	 * Attempts to load a temp file from designated path.
	 *
	 * @param file    Path to the file
	 * @return        True if load was successful, false otherwise
	 */
	bool loadTempFile(const boost::filesystem::path &file);

	//! @name Make operations/data available for Object hierarcy
	//@{
	virtual uint32_t getOperCount() const;
	virtual Object::Operation getOper(uint32_t n) const;
	virtual void doOper(const Operation &oper);
	//@}
};

#endif
