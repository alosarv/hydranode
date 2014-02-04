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
 * \file corefwd.h Forward declarations for Hydranode Core classes
 */

#ifndef __HNCOREFWD_H__
#define __HNCOREFWD_H__

#include <hnbase/osdep.h>
#include <hnbase/fwd.h>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>

// files management subsystem
class FilesList;
class SharedFile;
class PartData;
namespace Detail {
	class UsedRange;
	class LockedRange;
	typedef boost::shared_ptr<UsedRange  > UsedRangePtr;
	typedef boost::shared_ptr<LockedRange> LockedRangePtr;
}

// search API
class Search;
class SearchResult;
typedef boost::shared_ptr<Search>                  SearchPtr;
typedef boost::shared_ptr<SearchResult>            SearchResultPtr;
typedef boost::function<void (SearchPtr)>          SearchHandler;
typedef boost::function<bool (const std::string&)> LinkHandler;
typedef boost::function<bool (const std::string&)> FileHandler;

class Hydranode;

// metadata subsystem
//! Various file types
enum FileType {
	FT_UNKNOWN = 0,                             //!< Unknown/any
	FT_ARCHIVE = 1,                             //!< zip/arj/rar/gz/bz2
	FT_VIDEO,                                   //!< avi/mpeg/mpg/wmv
	FT_AUDIO,                                   //!< mp3/mpc/ogg
	FT_IMAGE,                                   //!< png/gif/jpg/bmp
	FT_DOCUMENT,                                //!< txt/doc/kwd/sxw/rtf
	FT_PROGRAM,                                 //!< exe/com/bat/sh
	FT_CDIMAGE                                  //!< iso/bin/cue/nrg
};
class MetaData;
class AudioMetaData;
class VideoMetaData;
class ArchiveMetaData;
class ImageMetaData;
class StreamData;

class HashSetMaker;
class ThreadWork;
class HashWork;
typedef boost::intrusive_ptr<ThreadWork> ThreadWorkPtr;
typedef boost::intrusive_ptr<HashWork> HashWorkPtr;

class ModuleBase;
class BaseClient;

/**
 * Events emitted by HashWork object when the job has been completed by
 * Hasher.
 */
enum HashEvent {
	HASH_COMPLETE    = 1,   //!< Full HashWork has been completed.
	HASH_VERIFIED    = 2,   //!< Range Hash has been verified.
	HASH_FAILED      = 3,   //!< Range Hash verification failed.
	HASH_FATAL_ERROR = 4    //!< Fatal error has occoured.
};

#endif
