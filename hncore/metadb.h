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
 * \file metadb.h Interface for MetaDb class
 */

#ifndef __METADB_H__
#define __METADB_H__

#include <hnbase/osdep.h>
#include <hnbase/hash.h>
#include <hncore/fwd.h>

//! For trace logging
#define METADB 1030
#define METADB_STR "MetaDb"

/**
 * MetaDb class is container for all Meta Data handled by this application.
 * It provides a number of lookup functions as well as addition functions
 * publically. Internally, it stores various cross-referenced lists to
 * provide fast lookups. The contents of this class are stored in metadb.xml,
 * and loaded on each runtime.
 *
 * The purpose of this class is to provide a application-central database
 * through which it is possible to perform cross-referencing between FilesList
 * and this list.
 */
class HNCORE_EXPORT MetaDb {
public:
	typedef std::set<MetaData*>::iterator Iter;
	typedef std::set<MetaData*>::const_iterator CIter;

	/**
	 * This class is a Singleton. The only instance of this class may
	 * be retrieved through this function. Note that the very first call
	 * to this function also initializes this class.
	 *
	 * @return The only instance of this class
	 */
	static MetaDb& instance();

	/**
	 * Load the MetaDb contents from input stream, adding all found entries
	 * in the stream to the database, merging duplicate entries.
	 *
	 * @param is       Input stream to read from.
	 */
	void load(std::istream &is);

	/**
	 * Write the contents of the database into output stream.
	 *
	 * @param os       Output stream to write to.
	 */
	void save(std::ostream &os) const;

	/**
	 * This should be called on application shutdown; cleans up all internal
	 * data.
	 */
	void exit();

	/**
	 * Add a metadata object to the database.
	 *
	 * @param md       MetaData pointer to be added to the database.
	 */
	void push(MetaData *md);

	/**
	 * Add a metadata object to the database, associating it with a
	 * SharedFile.
	 *
	 * @param md       MetaData pointer to be added to the database.
	 * @param id       SharedFile to associate with this data.
	 */
	void push(MetaData *md, SharedFile *sf);

	/**
	 * Find Metadata by searching with hash
	 *
	 * @param h        Reference to HashBase object to be searched for.
	 * @return         Pointer to the MetaData object, or 0 if not found.
	 */
	MetaData* find(const HashBase &h) const;

	/**
	 * Find MetaData by searching with file name. Note that this function
	 * may return any number of MetaData objects in case the filename is
	 * ambigious.
	 *
	 * @param filename Name of the file to search for
	 * @return         Vector containing all found entries. May be empty.
	 */
	std::vector<MetaData*> find(const std::string &filename) const;

	/**
	 * Find MetaData by searching with SharedFile
	 *
	 * @param id       SharedFile to search with
	 * @return         Pointer to found MetaData object, or 0 if not found.
	 */
	MetaData* find(SharedFile *sf) const;

	/**
	 * Locate SharedFile by searching with hash
	 *
	 * @param h        Hash to search for
	 * @return         The file being searched for, or 0 if not found.
	 */
	SharedFile* findSharedFile(const HashBase &h) const;

	/**
	 * Locate SharedFile by searching with file name. Note that this
	 * function may return any number of SharedFiles if the file name is
	 * ambigious.
	 *
	 * @param filename   Name of file to search for
	 * @return           Vector containing all found entries. May be empty.
	 */
	std::vector<SharedFile*> findSharedFile(
		const std::string &filename
	) const;

	/**
	 * Remove a SharedFile <-> MetaData association from the database
	 *
	 * @param sf         SharedFile to be removed
	 */
	void remSharedFile(SharedFile *sf);

	/**
	 * @returns const_iterator to beginning of the database
	 */
	CIter begin() const { return m_list.begin(); }

	/**
	 * @returns const_iterator to the end of the database
	 */
	CIter end() const { return m_list.end(); }

	/**
	 * @returns the size of the database
	 */
	size_t size() const { return m_list.size(); }
private:
	/**
	 * @name Singleton
	 */
	//@{
	MetaDb();
	MetaDb(MetaDb &);
	MetaDb& operator=(const MetaDb&);
	~MetaDb();
	//@}

	/**
	 * HashWrapper structure acts as a container for HashBase pointers in
	 * order to allow us to have a map of those keyed by the Hash.
	 */
	class HashWrapper {
	public:
		HashWrapper(const HashBase *obj) : m_object(obj) {}
		const HashBase *const m_object;           //!< Contained object

		//! Operator to work with std containers
		friend bool operator<(
			const HashWrapper &x, const HashWrapper &y
		) {
			return (*x.m_object) < (*y.m_object);
		}
		friend std::ostream& operator<<(
			std::ostream &o, const HashWrapper &hw
		) {
			return o << (*hw.m_object);
		}
		bool operator==(const HashWrapper &h) const {
			return *m_object == *h.m_object;
		}
		bool operator!=(const HashWrapper &h) const {
			return *m_object != *h.m_object;
		}
	private:
		HashWrapper();
	};

	/**
	 * Primary List
	 * ------------
	 * Pure metadata objects. We really cannot rely on any data existing in
	 * the contained MetaData pointer, so we can't turn this into some kind
	 * of map or anything. However, other lists contain iterators to this
	 * list for faster access - iterate directly on this list only as last
	 * resort. This list is also used to save in metadata.xml file, and is
	 * also ready for publishing to outside world.
	 */
	std::set<MetaData*> m_list;
	//! List Iterator
	typedef std::set<MetaData*>::iterator LIter;
	//! Constant List Iterator
	typedef std::set<MetaData*>::const_iterator CLIter;

	/**
	 * SharedFile To MetaData Map
	 * ----------------------
	 * SharedFile* keyed map to locate MetaData objects by searching with
	 * SharedFile pointer. This list is populated by FilesList (and
	 * contained) classes, and generally contains only part of the entire
	 * m_list contents - only entries which are also in FilesList are
	 * listed here.
	 */
	std::map<SharedFile*, MetaData*> m_sfToMd;
	//! SharedFile to MetaData Iterator
	typedef std::map<SharedFile*, MetaData*>::iterator SFMDIter;
	//! Constant SharedFile to MetaData Iterator
	typedef std::map<SharedFile*, MetaData*>::const_iterator CSFMDIter;

	/**
	 * File Name to MetaData Map
	 * -------------------------
	 * This map allows finding MetaData knowing file name. The file name
	 * key here is only that - file name. No paths may be included in the
	 * name. Multiple objects may have the same file name - for this
	 * reason, this is a multi-map.
	 */
	std::multimap<std::string, MetaData*> m_filenames;
	//! File Name Iterator
	typedef std::multimap<std::string, MetaData*>::iterator FNIter;
	//! Constant File Name Iterator
	typedef std::multimap<std::string, MetaData*>::const_iterator CFNIter;

	/**
	 * File Name to SharedFile Map
	 * ------------------------
	 * This map allows searching with file name and locating the
	 * corresponding SharedFile's. Note that this is a multimap - same file
	 * name may point to several SharedFiles.
	 */
	std::multimap<std::string, SharedFile*> m_nameToSF;
	//! Name To SharedFile Iterator
	typedef std::multimap<std::string, SharedFile*>::iterator NTSFIter;
	//! Constant Name To SharedFile Iterator
	typedef std::multimap<
		std::string, SharedFile*>
	::const_iterator CNTSFIter;

	/**
	 * Hash To SharedFile Map
	 * ----------------------
	 * A dual map which allows us to locate SharedFiles given a hash. The
	 * outer map is keyed on HashTypeId, so there are generally only 5-6
	 * entries there. For each of the outer maps, there's an inner map,
	 * containing hashes of the given type. Lookups in the inner map return
	 * the SharedFile of the searched entry.
	 */
	std::map<
		CGComm::HashTypeId, std::map<HashWrapper, SharedFile*>
	> m_hashToSF;
	//! Hash To SharedFile Iterator
	typedef std::map<
		CGComm::HashTypeId, std::map<HashWrapper, SharedFile*>
	>::iterator HTSFIter;
	//! Constant Hash To SharedFile Iterator
	typedef std::map<
		CGComm::HashTypeId, std::map<HashWrapper, SharedFile*>
	>::const_iterator CHTSFIter;
	//! HashWrapper To SharedFile Iterator
	typedef std::map<HashWrapper, SharedFile*>::iterator HWTSFIter;
	//! Constant HashWrapper To SharedFile Iterator
	typedef std::map<HashWrapper, SharedFile*>::const_iterator CHWTSFIter;

	/**
	 * Hash To Meta Data Map
	 * ---------------------
	 * Last, but not least, a recursive map which allows us to look up
	 * MetaData objects given we know one hash. The outer map is keyed
	 * on hash types, so there are generally only 5-6 entries in the
	 * outer map. For each of the outer map, there is an inner map, which
	 * is keyed on the actual Hash, and returns MetaData pointer. Note
	 * that we use HashWrapper wrapper class around HashBase pointer,
	 * doing virtual function calls on lookups. While it is known that
	 * virtual function calls come with some performance tradeoff, I
	 * really don't see other way of doing it. Also keep in mind that
	 * when HashSet object (contained in MetaData object) is destroyed,
	 * it would invalidate the HashWrapper pointer - make sure to not
	 * have any HashWrappers containing the HashBase objects around
	 * anymore when deleting MetaData.
	 */
	std::map<
		CGComm::HashTypeId, std::map<HashWrapper, MetaData*>
	> m_hashes;
	//! Hash To Meta Data Iterator
	typedef std::map<
		CGComm::HashTypeId, std::map<HashWrapper, MetaData*>
	>::iterator HTMDIter;
	//! Constant Hash To Meta Data Iterator
	typedef std::map<
		CGComm::HashTypeId, std::map<HashWrapper, MetaData*>
	>::const_iterator CHTMDIter;
	//! Hash Wrapper To Meta Data Iterator
	typedef std::map<HashWrapper, MetaData*>::iterator HWTMDIter;
	//! Constant Hash Wrapper To Meta Data Iterator
	typedef std::map<HashWrapper, MetaData*>::const_iterator CHWTMDIter;

	//! Output operator for streams
	friend std::ostream& operator<<(std::ostream &o, const MetaDb &md);

	/**
	 * Event handler for MetaData events, called from event table.
	 *
	 * @param md       Event source triggering the event.
	 * @param evt      Event data specifying the kind of event.
	 */
	void onMetaDataEvent(MetaData *md, int evt);

	/**
	 * Event handler for SharedFile events, called from event table.
	 *
	 * @param sf       SharedFile object triggering the event.
	 * @param evt      Event data specifying the kind of event.
	 */
	void onSharedFileEvent(SharedFile *sf, int evt);

	/**
	 * Attempt to add entry to m_filenames list.
	 *
	 * @param name     Filename to be added.
	 * @param source   Source object the file name belongs to.
	 */
	void tryAddFileName(MetaData *source, const std::string &name);

	/**
	 * Attempt to add entry to m_nameToId map
	 *
	 * @param name     Filename to be added.
	 * @param id       ID of the file the name belongs to.
	 */
	void tryAddFileName(SharedFile *sf, const std::string &name);

	/**
	 * Attempt to add entry to m_hashes list
	 *
	 * @param hash     Hash to be added.
	 * @param source   Source object where this HashSet belongs to.
	 */
	void tryAddHashSet(MetaData *source, const HashSetBase *hash);

	/**
	 * Attempt to add entry to m_hashToId map
	 *
	 * @param hash     Hash to be added.
	 * @param id       ID of the hash being added.
	 */
	void tryAddHashSet(SharedFile *sf, const HashSetBase *hash);

	/**
	 * Clears all lists. This is used for debugging purposes in metadata
	 * regress-test.
	 */
	void clear();
	friend void test_metadb();
};

#endif
