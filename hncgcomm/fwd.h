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

#ifndef __CGCOMMFWD_H__
#define __CGCOMMFWD_H__

#include <boost/shared_ptr.hpp>
namespace Engine {
	class Main;
	class Search;
	class SearchResult;
	class DownloadList;
	class DownloadInfo;
	class SharedFilesList;
	class SharedFile;
	class Config;
	class Log;
	class Modules;
	class Object;
	class WorkThread;
	class Networking;
	class SubSysBase;
	class Module;
	class Modules;
	class Object;

	typedef boost::shared_ptr<SearchResult> SearchResultPtr;
	typedef boost::shared_ptr<DownloadInfo> DownloadInfoPtr;
	typedef boost::shared_ptr<SharedFile>   SharedFilePtr;
	typedef boost::shared_ptr<Module>       ModulePtr;
	typedef boost::shared_ptr<Object>       ObjectPtr;

	enum FileType {
		FT_UNKNOWN = 0,                         //!< Unknown/any
		FT_ARCHIVE = 1,                         //!< zip/arj/rar/gz/bz2
		FT_VIDEO,                               //!< avi/mpeg/mpg/wmv
		FT_AUDIO,                               //!< mp3/mpc/ogg
		FT_IMAGE,                               //!< png/gif/jpg/bmp
		FT_DOC,                                 //!< txt/doc/kwd/sxw/rtf
		FT_PROG,                                //!< exe/com/bat/sh
		FT_CDDVD                                //!< iso/bin/cue/nrg
	};
}

#endif
