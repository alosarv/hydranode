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


#ifndef __ED2K_ED2K_H__
#define __ED2K_ED2K_H__

/**
 * \mainpage Hydranode ED2K Module
 * \date October 2004
 * \author Alo Sarv <madcat (at) hydranode (dot) com>
 *
 * ED2K Module performs communication with eDonkey2000 P2P network. The network
 * protocol implemented within this module is documented in \ref ed2kproto
 */

#include <hncore/ed2k/ed2ktypes.h>
#include <hnbase/hash.h>
#include <hncore/modules.h>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <stdexcept>

/**
 * All of ed2k module types and functions are located in this namespace.
 */
namespace Donkey {

class ED2K : public ModuleBase {
	DECLARE_MODULE(ED2K, "ed2k");
public:
	virtual bool onInit();
	virtual int onExit();
	virtual std::string getDesc() const;

	//! @name Accessors for various commonly-used variables in this module
	//@{
	uint32_t      getId()      const { return m_id;       }
	uint16_t      getTcpPort() const { return m_tcpPort;  }
	uint16_t      getUdpPort() const { return m_udpPort;  }
	Hash<MD4Hash> getHash()    const { return m_userHash; }
	std::string   getNick()    const { return m_nick;     }
	bool          isLowId()    const { return ::Donkey::isLowId(m_id);  }
	bool          isHighId()   const { return ::Donkey::isHighId(m_id); }
	boost::filesystem::path getConfigDir() const { return m_configDir; }

	void setId(uint32_t id) { m_id = id; } //!< Used by ServerList
	void setNick(const std::string  &nick);
	void setTcpPort(uint16_t port) { m_tcpPort = port; }
	void setUdpPort(uint16_t port) { m_udpPort = port; }
	//@}
protected:
	virtual void openUploadSlot();
private:
	// loads userhash from ed2k/userhash.dat, or creates one if needed
	void loadUserHash();
	//! Generates new userhash
	std::string createUserHash() const;
	//! Enforces limits
	void configChanged(const std::string &key, const std::string &value);
	void adjustLimits();

	//! Configuration directory
	boost::filesystem::path m_configDir;

	uint32_t    m_id;       //!< Own ID in ed2k network
	uint16_t    m_tcpPort;  //!< Where we have our TCP listener
	uint16_t    m_udpPort;  //!< Where we have our UDP listener
	std::string m_nick;     //!< Our nickname on ed2k net

	//! own userhash on ed2k network. This just a bunch of randomness
	//! really, but - its a hash afterall. Thus the object type.
	Hash<MD4Hash> m_userHash;

	//! Handler for string-based links for starting a download
	bool linkHandler(const std::string &link);

	/**
	 * Migrates settings from ed2k.ini to config.ini and destroys
	 * ed2k.ini if needed. This is a temporary function that should remain
	 * through 0.2 release series, but removed after that, when the
	 * migration to single config.ini is complete.
	 */
	void migrateSettings();

	/**
	 * Migrates user hash from config.ini to userhash.dat
	 */
	void migrateUserHash();

private:
	/**
	 * @name Operations forwarded to serverlist class
	 */
	//@{
	virtual uint32_t getOperCount() const;
	virtual Object::Operation getOper(uint32_t n) const;
	virtual void doOper(const Object::Operation &op);
	//@}
};

// ParseError exception class
// --------------------------
class ParseError : public std::runtime_error {
public:
	ParseError() : std::runtime_error("") {}
	ParseError(const std::string &msg) : std::runtime_error(msg) {}
	ParseError(boost::format fmt) : std::runtime_error(fmt.str()) {}
	~ParseError() throw() {}
};

} // end namespace Donkey

#endif
