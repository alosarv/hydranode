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
 * \file ed2k.cpp Implementation of ED2K Module entry point
 */

#include <hnbase/log.h>
#include <hnbase/schedbase.h>
#include <hnbase/prefs.h>
#include <hncore/ed2k/ed2k.h>
#include <hncore/ed2k/serverlist.h>
#include <hncore/ed2k/clientlist.h>
#include <hncore/ed2k/creditsdb.h>
#include <hncore/ed2k/downloadlist.h>
#include <hncore/hydranode.h>
#include <hncore/metadb.h>
#include <hncore/fileslist.h>
#include <hncore/sharedfile.h>
#include <hncore/metadata.h>
#include <boost/tokenizer.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>

namespace Donkey {

IMPLEMENT_MODULE(ED2K);

bool ED2K::onInit() {
	m_configDir = Hydranode::instance().getConfigDir()/"ed2k";
	logMsg(
		boost::format("ED2K configuration directory: %s") %
		m_configDir.native_directory_string()
	);
	Log::instance().addTraceMask("ed2k.tag");

	m_id = 0;

	using namespace boost::filesystem;

	if (!exists(m_configDir)) try {
		create_directory(m_configDir);
	} catch (filesystem_error &err) {
		logError(
			boost::format(
				"Unable to create folder for "
				"ed2k configuration: %s"
			) % err.what()
		);
		logError("Ed2k configuration settings will not be saved.");
	}
	if (!is_directory(m_configDir)) {
		logError("Nondirectory is blocking ed2k configuration ");
		logError(boost::format("at %s") % m_configDir.string());
		logError("ed2k configuration settings will not be saved.");
	}

	// Bring up data structures
	CreditsDb::instance().initCrypting();
	ServerList::instance().load((m_configDir/"server.met").string());
	CreditsDb::instance().load((m_configDir/"clients.met").string());

	// originally, ed2k module used it's own config file, ed2k/ed2k.ini
	// however, the current agreement is that modules should use a section
	// in main config file rather than their own config files. Hence, this
	// method should stay around throughout 0.2 release series, converting
	// the settings from the old ed2k.ini to config.ini if needed. This
	// migration should be removed for 0.3 release (or 3 months after 0.2
	// release, whichever comes latter).
	migrateSettings();

	// originally, ed2k UserHash was stored in config.ini, but now we want
	// to store it as separate file, so this migrates it.
	migrateUserHash();

	// Initialize userhash
	loadUserHash();

	// Set nickname
	std::string nick = Prefs::instance().read<std::string>("/ed2k/Nick","");
	if (nick.empty()) {
		char *usr = getenv("USER");
		if (usr == 0) {
			nick = "http://hydranode.com";
		} else {
			nick = usr;
			nick += " [http://hydranode.com]";
		}
	}
	setNick(nick);

	m_tcpPort = Prefs::instance().read("/ed2k/TCP Port", 4663);
	m_udpPort = Prefs::instance().read("/ed2k/UDP Port", 4673);

	// since fallback ports can result in m_*port being changed during
	// init, cache the value here, to avoid writing the new port to prefs
	int tcpPort = m_tcpPort;
	int udpPort = m_udpPort;

	adjustLimits();
	Prefs::instance().valueChanged.connect(
		boost::bind(&ED2K::configChanged, this, _1, _2)
	);

	// Initialize
	DownloadList::instance().init();
	ClientList::instance().init();
	ServerList::instance().init();

	// Finalize
	Search::addLinkHandler(boost::bind(&ED2K::linkHandler, this, _1));

	// save all settings, to create the conf file on first start
	if (m_tcpPort == tcpPort) {
		Prefs::instance().write("/ed2k/TCP Port", m_tcpPort);
	}
	if (m_udpPort == udpPort) {
		Prefs::instance().write("/ed2k/UDP Port", m_udpPort);
	}

	return true;
}

int ED2K::onExit() {
	if (m_tcpPort == Prefs::instance().read<uint32_t>("/ed2k/TCP Port", 0)){
		Prefs::instance().write("/ed2k/TCP Port", m_tcpPort);
	}
	if (m_udpPort == Prefs::instance().read<uint32_t>("/ed2k/UDP Port", 0)){
		Prefs::instance().write("/ed2k/UDP Port", m_udpPort);
	}

	ServerList::instance().save((m_configDir/"server.met").string());
	CreditsDb::instance().save((m_configDir/"clients.met").string());

	ClientList::instance().exit();
	DownloadList::instance().exit();
	ServerList::instance().exit();

	return 0;
}

bool ED2K::linkHandler(const std::string &link) try {
	typedef boost::char_separator<char> separator;
	separator sep("|");
	boost::tokenizer<separator> tok(link, sep);

	boost::tokenizer<separator>::iterator tok_iter = tok.begin();

	if (tok_iter == tok.end() || *tok_iter++ != "ed2k://") {
		return false;
	}
	if (tok_iter == tok.end() || *tok_iter++ != "file") {
		return false;
	}
	if (tok_iter == tok.end()) {
		logError("Incomplete ed2k:// link: Missing filename.");
		return false;
	}

	std::string name = *tok_iter++;

	if (tok_iter == tok.end()) {
		logError("Incomplete ed2k:// link: Missing filesize.");
		return false;
	}

	uint32_t size = boost::lexical_cast<uint32_t>(*tok_iter++);

	if (tok_iter == tok.end()) {
		logError("Incomplete ed2k:// link: Missing hash.");
		return false;
	}
	Hash<ED2KHash> hash(Utils::encode(*tok_iter++));

	SharedFile *sf = MetaDb::instance().findSharedFile(hash);
	if (sf) {
		std::string msg;
		if (sf->isPartial()) {
			msg += "You are already trying to download ";
		} else {
			msg += "You alread have file ";
		}
		msg += sf->getName();
		logMsg(msg);
		return false;
	}
	MetaData *md = new MetaData(size);
	md->addFileName(name);
	ED2KHashSet *hs = new ED2KHashSet(hash);

	// files smaller than chunksize have 1 chunkhash equal to filehash
	if (md->getSize() <= ED2K_PARTSIZE) {
		hs->addChunkHash(hash.getData());
	}

	md->addHashSet(hs);
	MetaDb::instance().push(md);
	FilesList::instance().createDownload(name, md);
	return true;
} catch (boost::bad_lexical_cast &) {
	logError("Error parsing ed2k:// link - wrong data type detected.");
	return false;
} catch (std::exception &e) {
	logError(boost::format("Error parsing ed2k:// link: %s") % e.what());
	return false;
}
MSVC_ONLY(;)

// Creates new userhash to be used in ed2k network. Note that hash[5] = 14 &&
// hash[14] = 111 is used by emule to identify emule clients, we shouldn't use
// that.
std::string ED2K::createUserHash() const {
	logDebug("Creating new eDonkey2000 userhash.");

	char tmp[16];
	for (uint8_t i = 0; i < 4; ++i) {
		uint32_t r = Utils::getRandom();
		memcpy(&tmp[i*4], reinterpret_cast<char*>(&r), 4);
	}

	// Mark us as eMule client (needed to be able to use eMule
	// extended protocol features)
	tmp[ 5] =  14;
	tmp[14] = 111;
	return std::string(tmp, 16);
}

void ED2K::openUploadSlot() {
	ClientList::instance().startNextUpload();
}

// we need to migrate 4 settings, but we don't want to migrate those settings
// which already have a value in the new config, so checks must be done to
// ensure we don't overwrite anything in new config. We only destroy the
// ed2k.ini if we migrated any settings from it, and even then, we back it up
// to ed2k.ini.bak just in case (it contains userhash, which is valuable)
void ED2K::migrateSettings() {
	boost::filesystem::path p(m_configDir/"ed2k.ini");
	Config &prefs = Prefs::instance();
	bool migrated = false;

	if (boost::filesystem::exists(p)) {
		Prefs::instance().setPath("/ed2k");
		Config c;
		c.load(p.native_file_string());

		std::string tmp = c.read<std::string>("UserHash", "");
		std::string tmp2 = prefs.read<std::string>("UserHash", "");
		if (tmp.size() && !tmp2.size()) {
			prefs.write<std::string>("UserHash", tmp);
			migrated = true;
		}

		tmp = c.read<std::string>("Nick", "");
		tmp2 = prefs.read<std::string>("Nick", "");
		if (tmp.size() && !tmp2.size()) {
			prefs.write<std::string>("Nick", tmp);
			migrated = true;
		}

		uint16_t tcpPort = c.read<uint16_t>("TCP Port", 0);
		uint16_t tcpPort2 = Prefs::instance().read<uint16_t>(
			"TCP Port", 0
		);
		if (tcpPort && !tcpPort2) {
			prefs.write<uint16_t>("TCP Port", tcpPort);
			migrated = true;
		}

		uint16_t udpPort = c.read<uint16_t>("UDP Port", 0);
		uint16_t udpPort2 = prefs.read<uint16_t>("UDP Port", 0);
		if (udpPort && !udpPort2) {
			prefs.write<uint16_t>("UDP Port", udpPort);
			migrated = true;
		}
	}
	if (migrated) {
		try {
			boost::filesystem::rename(
				p, p.native_file_string() + ".bak"
			);
		} catch (std::exception &) {} // silently ignored

		logMsg(COL_BGREEN
			"Info: eDonkey2000 settings have been "
			"moved to config.ini."
			COL_NONE
		);
		logMsg(COL_BGREEN
			"Info: Original ed2k.ini renamed to ed2k.ini.bak."
			COL_NONE
		);
	}
}

void ED2K::migrateUserHash() {
	std::string oldHash = Prefs::instance().read<std::string>(
		"/ed2k/UserHash", ""
	);
	boost::filesystem::path hashFile(getConfigDir()/"userhash.dat");
	if (oldHash.length() == 32 && !boost::filesystem::exists(hashFile)) {
		Hash<MD4Hash> uHash(Utils::encode(oldHash));
		std::ofstream ofs(
			hashFile.native_file_string().c_str(),
			std::ios::out|std::ios::binary
		);
		Utils::putVal<std::string>(ofs, uHash.getData(), 16);
		logMsg(
			"Info: Userhash moved from config.ini "
			"to ed2k/userhash.dat."
		);
	}
}

void ED2K::setNick(const std::string &nick) {
	Prefs::instance().write("/ed2k/Nick", nick);
	m_nick = nick;
}

void ED2K::loadUserHash() {
	using namespace Utils;
	bool created = false;
	boost::filesystem::path hFile(getConfigDir()/"userhash.dat");
	std::ifstream hashFile(
		hFile.native_file_string().c_str(), std::ios::binary
	);
	if (hashFile) try {
		m_userHash = Hash<MD4Hash>(hashFile);
	} catch (std::exception&) {}

	if (!m_userHash) {
		m_userHash = createUserHash();
		created = true;
	}
	
	if (!m_userHash) {
		throw std::runtime_error(
			"Fatal internal error: unable to create/load userhash"
		);
	}
	if (m_userHash.getData()[5] != 14 || m_userHash.getData()[14] != 111) {
		// First versions of HN didn't create eMule-type hash - recreate
		logMsg("Re-creating eMule-compatible userhash.");
		m_userHash = createUserHash();
		created = true;
	}
	if (created) {
		std::ofstream ofs(
			hFile.native_file_string().c_str(), std::ios::binary
		);
		putVal<std::string>(ofs, m_userHash.getData(), 16);
	}
	logDebug(boost::format("Own user hash: %s") % m_userHash.decode());
}

void ED2K::configChanged(const std::string &key, const std::string &value) {
	if (key == "UpSpeedLimit" || key == "DownSpeedLimit") {
		adjustLimits();
	}
}

void ED2K::adjustLimits() {
	boost::format fmt(
		"Lowering download speed limit to %s/s due "
		"to ED2K network netiquette."
	);
	uint32_t uLimit = Prefs::instance().read("/UpSpeedLimit", 0);
	uint32_t dLimit = Prefs::instance().read("/DownSpeedLimit", 0);
	if (!dLimit) {
		dLimit = std::numeric_limits<uint32_t>::max();
	} 
	if (!uLimit) {
		uLimit = std::numeric_limits<uint32_t>::max();
	}

	if (uLimit < 4 * 1024 && dLimit > uLimit * 3) {
		logMsg(fmt % Utils::bytesToString(uLimit * 3));
		Prefs::instance().write<uint32_t>("/DownSpeedLimit", uLimit*3);
	} else if (uLimit < 10 * 1024 && dLimit > uLimit * 4) {
		logMsg(fmt % Utils::bytesToString(uLimit * 4));
		Prefs::instance().write<uint32_t>("/DownSpeedLimit", uLimit*4);
	}
}

std::string ED2K::getDesc() const { 
	return "eDonkey2000";
}

uint32_t ED2K::getOperCount() const {
	return ServerList::instance().getOperCount();
}
Object::Operation ED2K::getOper(uint32_t n) const {
	return ServerList::instance().getOper(n);
}
void ED2K::doOper(const Object::Operation &op) {
	return ServerList::instance().doOper(op);
}

} // end namespace Donkey
