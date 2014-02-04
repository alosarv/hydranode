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
 * \file creditsdb.h Interface for CreditsDb and ClientCredits classes
 */

#ifndef __ED2K_CREDITSDB_H__
#define __ED2K_CREDITSDB_H__

#include <hncore/ed2k/ed2ktypes.h>
#include <hncore/ed2k/publickey.h>
#include <hnbase/object.h>
#include <hnbase/hash.h>
#include <math.h>

namespace Donkey {

/**
 * Credits object represent one single client's credits entry.
 */
class Credits {
public:
	//! Construct by publickey and hash
	Credits(PublicKey key, const Hash<MD4Hash> &h);

	float getScore() const {
		if (m_downloaded < 1024*1024) {
			return 1.0;
		}
		float score1 = m_uploaded ? m_downloaded * 2 / m_uploaded : 10;
		float score2 = sqrt(m_downloaded / (1024.0 * 1024.0) + 2.0);
		if (score1 > score2) {
			score1 = score2;
		}
		if (score1 < 1.0) {
			return 1.0;
		} else if (score1 > 10.0) {
			return 10.0;
		} else {
			return score1;
		}
	}

	uint64_t      getUploaded()   const { return m_uploaded;   }
	uint64_t      getDownloaded() const { return m_downloaded; }
	uint32_t      getLastSeen()   const { return m_lastSeen;   }
	Hash<MD4Hash> getHash()       const { return m_hash;       }
	PublicKey     getPubKey()     const { return m_pubKey;     }

	void addUploaded(uint32_t amount)   { m_uploaded += amount;   }
	void addDownloaded(uint32_t amount) { m_downloaded += amount; }
	// notice: 32bit value, this in seconds, not milliseconds
	void setLastSeen(uint32_t time)     { m_lastSeen = time;      }
private:
	friend class CreditsDb;

	/**
	 * Construct and load from stream. This is used only by
	 * CreditsDb, and is thus private.
	 *
	 * @param i          Input stream to read data from
	 * @param ver        Version of stream (pubkey or not)
	 */
	Credits(std::istream &i, uint8_t ver);

	Credits();                                    //!< Forbidden
	~Credits();                                   //!< Allowed by CreditsDb
	Credits(const Credits&);                      //!< Forbidden
	const Credits& operator=(const Credits&);     //! Forbidden

	//! @name Data
	//@{
	Hash<MD4Hash> m_hash;              //!< Userhash
	uint64_t      m_uploaded;          //!< Sent to him/her
	uint64_t      m_downloaded;        //!< Received from him/her
	uint32_t      m_lastSeen;          //!< Time last seen
	PublicKey     m_pubKey;            //!< public key
	//@}

	//! Output operator to streams, used for writing clients.met
	friend std::ostream& operator<<(std::ostream &o, const Credits &c);
};

namespace Detail {
	struct CreditsList;
}

/**
 * CreditsDb class stores and maintains ClientCredits type objects, which
 * represent eDonkey2000 client credits. Actually, what this means is we
 * store the clients userhash and public key, and keep track of how much we
 * have sent data to the client, and how much we have received back. All of
 * this forms the base for eMule extended protocol feature, which rewards
 * uploaders based on their credits. More on that in UploadQueue-related
 * classes. Note that not all clients connecting to eDonkey2000 network support
 * credits, and thus only clients giving back credits should be rewarded here.
 * At the point of this writing (15/10/2004), only eMule-derived clients, plus
 * ShareAza fully support this system as far as I know.
 *
 * CreditsDb stores its contents at config/ed2k/clients.met between sessions.
 * The file format conforms to eMule's respective file format.
 *
 * This class is a Singleton, the only instance of this class may be retrieved
 * through instance() member function.
 */
class CreditsDb {
public:
	//! Singleton, lazy instanciating
	static CreditsDb& instance() {
		static CreditsDb *cdb = new CreditsDb();
		return *cdb;
	}

	/**
	 * Initializes public/private RSA keypair either by loading it from
	 * file, or creating new one if needed.
	 */
	void initCrypting();

	/**
	 * Load the contents from file, adding all entries found there to list
	 *
	 * @param file     File to read data from
	 *
	 * \throws std::runtime_error if parsing fails
	 */
	void load(const std::string &file);

	/**
	 * Save the contents to file.
	 *
	 * @param file      File to write to
	 */
	void save(const std::string &file) const;

	/**
	 * @returns Own public key
	 */
	PublicKey getPublicKey() const { return m_pubKey; }

	/**
	 * Find credits, looking with PublicKey.
	 *
	 * @param key  Client's PublicKey to search for
	 * @return     Pointer to Credits object corresponding to @param key, or
	 *             0 if not found.
	 */
	Credits* find(const PublicKey &key) const;

	/**
	 * Find credits, looking with userhash.
	 *
	 * @param hash  Client's userhash
	 * @return      Pointer to Credits object corresponding to @param hash,
	 *              or 0 if not found.
	 *
	 * \note Usage of this function is discouraged, due to hash-stealers.
	 */
	Credits* find(const Hash<MD4Hash> &hash) const;

	/**
	 * Create credits entry for specified publickey/hash
	 */
	Credits* create(PublicKey key, const Hash<MD4Hash> &hash);

	/**
	 * Creates a signature to be sent to client owning target credits.
	 *
	 * @param key        Remote client's public key
	 * @param callenge   Challenge value
	 * @param ipType     Type of IP (if any) to include
	 * @param ip         The ip to be included (if any)
	 * @return           Signature to be sent back to client
	 */
	static std::string createSignature(
		PublicKey key, uint32_t challenge, IpType ipType, uint32_t ip
	);

	/**
	 * Attempts to verify signature against public key stored here.
	 *
	 * @param key        Remote client's public key
	 * @param challenge  Challenge value sent to this client previously
	 * @param sign       The signature the remote client sent back
	 * @param ipType     Type of IP included in the signature (if any)
	 * @param ip         IP address included in the signature (if any)
	 * @return           True if verification succeeds, false otherwise
	 */
	static bool verifySignature(
		PublicKey key, uint32_t challenge,
		const std::string &sign, IpType ipType, uint32_t ip
	);
private:
	CreditsDb();                                   //!< Singleton
	~CreditsDb();                                  //!< Singleton
	CreditsDb(const CreditsDb&);                   //!< No copying allowed
	const CreditsDb& operator=(const CreditsDb&);  //!< No copying allowed

	/**
	 * Loads private RSA key from file, and calculates public RSA key from
	 * it.
	 *
	 * @param where      File to read private key from.
	 */
	void loadCryptKey(const std::string &where);

	/**
	 * Creates new public/private RSA keypair, and saves it to file.
	 *
	 * @param where      File to write private key to.
	 */
	void createCryptKey(const std::string &where);

	//! Public RSA key
	PublicKey m_pubKey;

	boost::scoped_ptr<Detail::CreditsList> m_list;
};

} // end namespace Donkey

#endif
