/*
 *  Copyright (C) 2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#include "filetypes.h"
#include <map>
#include <string>
#include <boost/algorithm/string/case_conv.hpp>
//#include <QDir>
#include <QSettings>
#include <QProcess>

using namespace Engine;

std::map<std::string, FileType> s_fileTypes;
extern void doLogDebug(const std::string &msg);
inline void logDebug(const QString &msg) { doLogDebug(msg.toStdString()); }

void initFileTypes() {
	s_fileTypes["669"]  = FT_AUDIO;
	s_fileTypes["aac"]  = FT_AUDIO;
	s_fileTypes["aif"]  = FT_AUDIO;
	s_fileTypes["aiff"] = FT_AUDIO;
	s_fileTypes["amf"]  = FT_AUDIO;
	s_fileTypes["ams"]  = FT_AUDIO;
	s_fileTypes["ape"]  = FT_AUDIO;
	s_fileTypes["au"]   = FT_AUDIO;
	s_fileTypes["dbm"]  = FT_AUDIO;
	s_fileTypes["dmf"]  = FT_AUDIO;
	s_fileTypes["dsm"]  = FT_AUDIO;
	s_fileTypes["far"]  = FT_AUDIO;
	s_fileTypes["flac"] = FT_AUDIO;
	s_fileTypes["it"]   = FT_AUDIO;
	s_fileTypes["m4a"]  = FT_AUDIO;
	s_fileTypes["mdl"]  = FT_AUDIO;
	s_fileTypes["med"]  = FT_AUDIO;
	s_fileTypes["mid"]  = FT_AUDIO;
	s_fileTypes["midi"] = FT_AUDIO;
	s_fileTypes["mka"]  = FT_AUDIO;
	s_fileTypes["mod"]  = FT_AUDIO;
	s_fileTypes["mol"]  = FT_AUDIO;
	s_fileTypes["mp1"]  = FT_AUDIO;
	s_fileTypes["mp2"]  = FT_AUDIO;
	s_fileTypes["mp3"]  = FT_AUDIO;
	s_fileTypes["mp4"]  = FT_AUDIO;
	s_fileTypes["mpa"]  = FT_AUDIO;
	s_fileTypes["mpc"]  = FT_AUDIO;
	s_fileTypes["mpp"]  = FT_AUDIO;
	s_fileTypes["mtm"]  = FT_AUDIO;
	s_fileTypes["nst"]  = FT_AUDIO;
	s_fileTypes["ogg"]  = FT_AUDIO;
	s_fileTypes["okt"]  = FT_AUDIO;
	s_fileTypes["psm"]  = FT_AUDIO;
	s_fileTypes["ptm"]  = FT_AUDIO;
	s_fileTypes["ra"]   = FT_AUDIO;
	s_fileTypes["rmi"]  = FT_AUDIO;
	s_fileTypes["s3m"]  = FT_AUDIO;
	s_fileTypes["stm"]  = FT_AUDIO;
	s_fileTypes["ult"]  = FT_AUDIO;
	s_fileTypes["umx"]  = FT_AUDIO;
	s_fileTypes["wav"]  = FT_AUDIO;
	s_fileTypes["wma"]  = FT_AUDIO;
	s_fileTypes["wow"]  = FT_AUDIO;
	s_fileTypes["xm"]   = FT_AUDIO;

	s_fileTypes["asf"]  = FT_VIDEO;
	s_fileTypes["avi"]  = FT_VIDEO;
	s_fileTypes["divx"] = FT_VIDEO;
	s_fileTypes["m1v"]  = FT_VIDEO;
	s_fileTypes["m2v"]  = FT_VIDEO;
	s_fileTypes["mkv"]  = FT_VIDEO;
	s_fileTypes["mov"]  = FT_VIDEO;
	s_fileTypes["mp1v"] = FT_VIDEO;
	s_fileTypes["mp2v"] = FT_VIDEO;
	s_fileTypes["mpe"]  = FT_VIDEO;
	s_fileTypes["mpeg"] = FT_VIDEO;
	s_fileTypes["mpg"]  = FT_VIDEO;
	s_fileTypes["mps"]  = FT_VIDEO;
	s_fileTypes["mpv"]  = FT_VIDEO;
	s_fileTypes["mpv1"] = FT_VIDEO;
	s_fileTypes["mpv2"] = FT_VIDEO;
	s_fileTypes["ogm"]  = FT_VIDEO;
	s_fileTypes["qt"]   = FT_VIDEO;
	s_fileTypes["ram"]  = FT_VIDEO;
	s_fileTypes["rm"]   = FT_VIDEO;
	s_fileTypes["rv"]   = FT_VIDEO;
	s_fileTypes["rv9"]  = FT_VIDEO;
	s_fileTypes["ts"]   = FT_VIDEO;
	s_fileTypes["vivo"] = FT_VIDEO;
	s_fileTypes["vob"]  = FT_VIDEO;
	s_fileTypes["wmv"]  = FT_VIDEO;
	s_fileTypes["xvid"] = FT_VIDEO;


	s_fileTypes["bmp"]  = FT_IMAGE;
	s_fileTypes["dcx"]  = FT_IMAGE;
	s_fileTypes["emf"]  = FT_IMAGE;
	s_fileTypes["gif"]  = FT_IMAGE;
	s_fileTypes["ico"]  = FT_IMAGE;
	s_fileTypes["jpeg"] = FT_IMAGE;
	s_fileTypes["jpg"]  = FT_IMAGE;
	s_fileTypes["pct"]  = FT_IMAGE;
	s_fileTypes["pcx"]  = FT_IMAGE;
	s_fileTypes["pic"]  = FT_IMAGE;
	s_fileTypes["pict"] = FT_IMAGE;
	s_fileTypes["png"]  = FT_IMAGE;
	s_fileTypes["psd"]  = FT_IMAGE;
	s_fileTypes["psp"]  = FT_IMAGE;
	s_fileTypes["tga"]  = FT_IMAGE;
	s_fileTypes["tif"]  = FT_IMAGE;
	s_fileTypes["tiff"] = FT_IMAGE;
	s_fileTypes["wmf"]  = FT_IMAGE;
	s_fileTypes["xif"]  = FT_IMAGE;


	s_fileTypes["7z"]  = FT_ARCHIVE;
	s_fileTypes["ace"] = FT_ARCHIVE;
	s_fileTypes["alz"] = FT_ARCHIVE;
	s_fileTypes["arj"] = FT_ARCHIVE;
	s_fileTypes["bz2"] = FT_ARCHIVE;
	s_fileTypes["cab"] = FT_ARCHIVE;
	s_fileTypes["cbz"] = FT_ARCHIVE;
	s_fileTypes["cbr"] = FT_ARCHIVE;
	s_fileTypes["gz"]  = FT_ARCHIVE;
	s_fileTypes["hqx"] = FT_ARCHIVE;
	s_fileTypes["lha"] = FT_ARCHIVE;
	s_fileTypes["lzh"] = FT_ARCHIVE;
	s_fileTypes["msi"] = FT_ARCHIVE;
	s_fileTypes["rar"] = FT_ARCHIVE;
	s_fileTypes["sea"] = FT_ARCHIVE;
	s_fileTypes["sit"] = FT_ARCHIVE;
	s_fileTypes["tar"] = FT_ARCHIVE;
	s_fileTypes["tgz"] = FT_ARCHIVE;
	s_fileTypes["uc2"] = FT_ARCHIVE;
	s_fileTypes["z"]   = FT_ARCHIVE;
	s_fileTypes["zip"] = FT_ARCHIVE;


	s_fileTypes["bat"] = FT_PROG;
	s_fileTypes["cmd"] = FT_PROG;
	s_fileTypes["com"] = FT_PROG;
	s_fileTypes["exe"] = FT_PROG;

	s_fileTypes["bin"]   = FT_CDDVD;
	s_fileTypes["bwa"]   = FT_CDDVD;
	s_fileTypes["bwi"]   = FT_CDDVD;
	s_fileTypes["bws"]   = FT_CDDVD;
	s_fileTypes["bwt"]   = FT_CDDVD;
	s_fileTypes["ccd"]   = FT_CDDVD;
	s_fileTypes["cue"]   = FT_CDDVD;
	s_fileTypes["dmg"]   = FT_CDDVD;
	s_fileTypes["dmz"]   = FT_CDDVD;
	s_fileTypes["img"]   = FT_CDDVD;
	s_fileTypes["iso"]   = FT_CDDVD;
	s_fileTypes["mdf"]   = FT_CDDVD;
	s_fileTypes["mds"]   = FT_CDDVD;
	s_fileTypes["nrg"]   = FT_CDDVD;
	s_fileTypes["sub"]   = FT_CDDVD;
	s_fileTypes["toast"] = FT_CDDVD;

	s_fileTypes["chm"]  = FT_DOC;
	s_fileTypes["css"]  = FT_DOC;
	s_fileTypes["diz"]  = FT_DOC;
	s_fileTypes["doc"]  = FT_DOC;
	s_fileTypes["dot"]  = FT_DOC;
	s_fileTypes["hlp"]  = FT_DOC;
	s_fileTypes["htm"]  = FT_DOC;
	s_fileTypes["html"] = FT_DOC;
	s_fileTypes["nfo"]  = FT_DOC;
	s_fileTypes["pdf"]  = FT_DOC;
	s_fileTypes["pps"]  = FT_DOC;
	s_fileTypes["ppt"]  = FT_DOC;
	s_fileTypes["ps"]   = FT_DOC;
	s_fileTypes["rtf"]  = FT_DOC;
	s_fileTypes["wri"]  = FT_DOC;
	s_fileTypes["txt"]  = FT_DOC;
	s_fileTypes["xls"]  = FT_DOC;
	s_fileTypes["xml"]  = FT_DOC;
}


FileType getFileType(const std::string &fileName) {
	if (!s_fileTypes.size()) {
		initFileTypes();
	}

	std::string ext = getExtension(fileName);
	std::map<std::string, FileType>::iterator it = s_fileTypes.find(ext);
	return it == s_fileTypes.end() ? FT_UNKNOWN : it->second;
}

std::string getExtension(const std::string &fileName) {
	std::string ext;
	size_t epos = fileName.find_last_of('.');
	if (epos == std::string::npos) {
		if (fileName.size() >= 3) {
			ext = fileName.substr(fileName.size() - 3);
		} else {
			ext = fileName;
		}
	} else {
		ext = fileName.substr(epos + 1);
	}

	boost::algorithm::to_lower(ext);

	return ext;
}

QString cleanName(QString curName) {
	curName = curName.simplified();
	int extDot = curName.lastIndexOf('.');
	curName = curName.replace('.', ' ');
	curName = curName.replace('_', ' ');
	curName[extDot] = '.';
	return curName.simplified();
}

void openExternally(QString loc) {
#ifdef Q_OS_WIN32
//	loc = QDir().absoluteFilePath(loc);
//	loc = loc.replace("/", "\\"); // Qt uses forward slashes :(
	QString ext = QString::fromStdString(getExtension(loc.toStdString()));
	logDebug("location=" + loc + ", extension=" + ext);
	QSettings conf(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\." + ext,
		QSettings::NativeFormat
	);
	QString appName = conf.value("").toString();
	if (!appName.size()) {
		logDebug("No appname found for file " + loc);
		logDebug("(note: extension is '" + ext + "')");
		return;
	}
	QSettings conf2(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\" + appName 
		+ "\\shell\\open\\command", QSettings::NativeFormat
	);
	QString appExe = conf2.value("").toString();
	if (!appExe.size()) {
		logDebug("No appExe found for file " + loc);
		logDebug("(note: extension is '" + ext + "')");
		return;
	}
	if (appExe.contains("%1")) {
		appExe.replace("%1", loc);
	} else if (appExe.contains("%L")) {
		appExe.replace("%L", loc);
	}
	logDebug("Starting process " + appExe);
	QProcess::startDetached(appExe);
#endif
}
