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
 * \file creditsdb.cpp Implementation of CreditsDb and Credits classes
 */

#include <hncore/ed2k/creditsdb.h>
#include <hncore/ed2k/ed2k.h>
#include <hncore/ed2k/cryptopp.h>
#include <hnbase/utils.h>
#include <hnbase/timed_callback.h>
#include <boost/filesystem/operations.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace Donkey {

//! Tags used in clients.met file
enum ED2K_ClientsMet {
	CM_VER   = 0x11,          //!< Old version - no RSA pub key
	CM_VER29 = 0x12           //!< New version (eMule 0.29+), with RSA key
};

//! The constant-size space alloted to saving keys in credits.met files (v29+).
const unsigned int ED2K_MaxKeySize = 80;

// Credits class
// -------------
Credits::Credits(PublicKey key, const Hash<MD4Hash> &h) : m_hash(h),
m_uploaded(), m_downloaded(), m_lastSeen(), m_pubKey(key) {
	CHECK_THROW(key.size() <= ED2K_MaxKeySize);
	CHECK_THROW(h);
}

// Dummy destructor
Credits::~Credits() {}

// Construct and load
Credits::Credits(std::istream &i, uint8_t ver) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	uint32_t uploadLow = Utils::getVal<uint32_t>(i);
	uint32_t downloadLow = Utils::getVal<uint32_t>(i);
	m_lastSeen = Utils::getVal<uint32_t>(i);
	uint32_t uploadHigh = Utils::getVal<uint32_t>(i);
	uint32_t downloadHigh = Utils::getVal<uint32_t>(i);

	// We prefer storing things as 64-bit values internally
	m_uploaded = uploadHigh;
	m_uploaded <<= 32;
	m_uploaded += uploadLow;
	m_downloaded = downloadHigh;
	m_downloaded <<= 32;
	m_downloaded += downloadLow;
	i.seekg(2, std::ios::cur); // Reserved bytes

	if (ver == CM_VER29) {
		uint8_t keySize = Utils::getVal<uint8_t>(i);
		CHECK_THROW(keySize <= ED2K_MaxKeySize);

		m_pubKey = PublicKey(Utils::getVal<std::string>(i, keySize));

		// key may be smaller than ED2K_MaxKeySize - seek forward then
		i.seekg(ED2K_MaxKeySize - keySize, std::ios::cur);
	}
	// revisions before 2441 didn't set m_lastSeen correctly
	if (!m_lastSeen) {
		m_lastSeen = EventMain::instance().getTick() / 1000;
	}
}

// Write to stream
std::ostream& operator<<(std::ostream &o, const Credits &c) {
	Utils::putVal<std::string>(o, c.m_hash.getData(), 16);
	Utils::putVal<uint32_t>(o, c.m_uploaded);
	Utils::putVal<uint32_t>(o, c.m_downloaded);
	Utils::putVal<uint32_t>(o, c.m_lastSeen);
	Utils::putVal<uint32_t>(o, c.m_uploaded >> 32);
	Utils::putVal<uint32_t>(o, c.m_downloaded >> 32);
	Utils::putVal<uint16_t>(o, 0); // 2 reserved bytes

	Utils::putVal<uint8_t>(o, c.m_pubKey.size());
	Utils::putVal<std::string>(o, c.m_pubKey.c_str(), c.m_pubKey.size());

	// add padding if keysize is < 80 (required for compatibility)
	if ( c.m_pubKey.size() < ED2K_MaxKeySize ) {
		std::string dummyStr(ED2K_MaxKeySize - c.m_pubKey.size(), '\0');
		Utils::putVal<std::string>(o, dummyStr, dummyStr.size());
	}

	return o;
}

namespace Detail {
	struct CreditsDbIndices : public boost::multi_index::indexed_by<
		boost::multi_index::ordered_unique<
			boost::multi_index::identity<Credits*>
		>,
		boost::multi_index::ordered_non_unique<
			boost::multi_index::const_mem_fun<
				Credits, Hash<MD4Hash>, &Credits::getHash
			>
		>,
		boost::multi_index::ordered_non_unique<
			boost::multi_index::const_mem_fun<
				Credits, PublicKey, &Credits::getPubKey
			>
		>
	> {};
	struct CreditsList : boost::multi_index_container<
		Credits*, CreditsDbIndices
	> {};
	enum CreditsListKeys { ID_Id, ID_Hash, ID_PubKey, ID_LastSeen };
	typedef CreditsList::nth_index<ID_Id    >::type::iterator IDIter;
	typedef CreditsList::nth_index<ID_Hash  >::type::iterator HashIter;
	typedef CreditsList::nth_index<ID_PubKey>::type::iterator KeyIter;
}
using namespace Detail;

// CreditsDb class
// ---------------
// Defined here to avoid including Crypto headers from creditsdb.h
typedef CryptoPP::RSASSA_PKCS1v15_SHA_Signer   Signer;
typedef CryptoPP::RSASSA_PKCS1v15_SHA_Verifier Verifier;
boost::shared_ptr<Signer>   s_signer;   //!< SecIdent: Signer
boost::shared_ptr<Verifier> s_verifier; //!< SecIdent: Verifier

// construction/destruction
CreditsDb::CreditsDb() : m_list(new CreditsList) {}
CreditsDb::~CreditsDb() {}

void CreditsDb::load(const std::string &file) try {
	std::ifstream ifs(file.c_str(), std::ios::binary);
	if (!ifs) {
		return; // Nothing to do
	}
	uint8_t ver = Utils::getVal<uint8_t>(ifs);
	if (ver != CM_VER && ver != CM_VER29) {
		logError(
			boost::format("Corruption found in credits database.")
		);
		return;
	}
	uint32_t count = Utils::getVal<uint32_t>(ifs);
	Utils::StopWatch t;
	uint32_t cleaned = 0;
	uint32_t expireValue = Utils::getTick() / 1000;
	expireValue -= 60 * 60 * 24 * 30 * 5; // 5 months in seconds
	while (ifs && count--) {
		Credits *c = new Credits(ifs, ver);
		if (c->getLastSeen() && c->getLastSeen() < expireValue) {
			delete c;
			++cleaned;
		} else {
			m_list->insert(c);
		}
	}
	logMsg(
		boost::format("CreditsDb loaded, %d clients are known (%dms)")
		% m_list->size() % t
	);
	if (cleaned) {
		logMsg(
			boost::format(
				"Cleaned %d clients (not seen for more "
				"than 5 months)"
			) % cleaned
		);
	}
	// save every 12 minutes
	Utils::timedCallback(
		boost::bind(&CreditsDb::save, this, file), 60*1000*12
	);
} catch (std::exception &e) {
	logError(boost::format("Error loading CreditsDb: %s") % e.what());
	logMsg("Attempting to load from backup...");
	try {
		load(file + ".bak");
		logMsg("Loading CreditsDb from backup succeeded.");
	} catch (std::exception &) {
		logError("Fatal: Failed to load CreditsDb from backup.");
	}
}
MSVC_ONLY(;)

void CreditsDb::save(const std::string &file) const {
	std::ofstream ofs(file.c_str(), std::ios::binary);
	if (!ofs) {
		logWarning(
			boost::format("Failed to open %s for saving CreditsDb.")
			% file
		);
		return;
	}
	Utils::putVal<uint8_t>(ofs, CM_VER29);
	Utils::putVal<uint32_t>(ofs, m_list->size());
	Utils::StopWatch t;
	for (IDIter i = m_list->begin(); i != m_list->end(); ++i) {
		ofs << *(*i);
	}

	logMsg(
		boost::format("CreditsDb saved, %d clients written (%sms)")
		% m_list->size() % t
	);
	// save every 12 minutes
	Utils::timedCallback(
		boost::bind(&CreditsDb::save, this, file), 60*1000*12
	);
}

Credits* CreditsDb::find(const PublicKey &key) const {
	CHECK_THROW(key.size());
	KeyIter it = m_list->get<ID_PubKey>().find(key);
	return it == m_list->get<ID_PubKey>().end() ? 0 : *it;
}

Credits* CreditsDb::find(const Hash<MD4Hash> &hash) const {
	CHECK_THROW(hash);
	HashIter it = m_list->get<ID_Hash>().find(hash);
	return it == m_list->get<ID_Hash>().end() ? 0 : *it;
}

void CreditsDb::initCrypting() {
	using namespace boost::filesystem;

	path keyPath = ED2K::instance().getConfigDir()/"cryptkey.dat";

	if (!exists(keyPath)) {
		createCryptKey(keyPath.native_file_string());
	}

	try {
		loadCryptKey(keyPath.native_file_string());
	} catch (std::exception &) {
		logWarning(
			"Failed to load personal RSA key; creating new one."
		);
		createCryptKey(keyPath.native_file_string());
		loadCryptKey(keyPath.native_file_string());
	}
}

void CreditsDb::loadCryptKey(const std::string &where) {
	using namespace CryptoPP;

	// load private key
	FileSource src(where.c_str(), true, new Base64Decoder);
	s_signer.reset(new Signer(src));
	s_verifier.reset(new Verifier(*s_signer));

	// pubkey itself
	boost::scoped_array<uint8_t> tmp(new uint8_t[80]);
	ArraySink sink(tmp.get(), 80);
	s_verifier->DEREncode(sink);
	sink.MessageEnd();

	uint8_t keySize = sink.TotalPutLength();
	m_pubKey = std::string(reinterpret_cast<char*>(tmp.get()), keySize);

	logMsg("RSA keypair loaded successfully.");
}

void CreditsDb::createCryptKey(const std::string &where) {
	using namespace CryptoPP;

	AutoSeededRandomPool rng;
	InvertibleRSAFunction privKey;
	privKey.Initialize(rng, 384); // 384-bit encryption
	Base64Encoder privKeySink(new FileSink(where.c_str()));
	privKey.DEREncode(privKeySink);
	privKeySink.MessageEnd();

	logMsg("Created new RSA keypair");
}

// creates a signature to be sent to remote client
// All the typecasting in here is needed, 'cos CryptoPP library wants uint8_t*
// type input, but we are always working with std::string, which only accepts
// sint8_t type input.
std::string CreditsDb::createSignature(
	PublicKey key, uint32_t challenge, IpType ipType, uint32_t ip
) {
	CHECK_THROW(key);

	// construct the message
	std::ostringstream tmp;
	Utils::putVal<std::string>(tmp, key.c_str(), key.size());
	Utils::putVal<uint32_t>(tmp, challenge);
	if (ipType) {
		Utils::putVal<uint32_t>(tmp, ip);
		Utils::putVal<uint8_t>(tmp, ipType);
	}

	std::string msg(tmp.str());
	const uint8_t *msgPtr = reinterpret_cast<const uint8_t*>(msg.c_str());

	// sign the message
	CryptoPP::SecByteBlock sign(s_signer->SignatureLength());
	CryptoPP::AutoSeededRandomPool rng;
	s_signer->SignMessage(rng, msgPtr, msg.size(), sign.begin());

	boost::scoped_array<uint8_t> out(new uint8_t[200]);
	CryptoPP::ArraySink asink(out.get(), 200);
	asink.Put(sign.begin(), sign.size());

	const char *retPtr = reinterpret_cast<const char*>(out.get());
	return std::string(retPtr, asink.TotalPutLength());
}

bool CreditsDb::verifySignature(
	PublicKey key, uint32_t challenge, const std::string &sign,
	IpType ipType, uint32_t ip
) {
	CHECK_THROW(key);
	CHECK_THROW(challenge);
	CHECK_THROW(sign.size());

	CryptoPP::StringSource sPKey(key.c_str(), key.size(), true, 0);
	Verifier verifier(sPKey);

	std::ostringstream tmp;
	Utils::putVal<std::string>(
		tmp, instance().m_pubKey.c_str(), instance().m_pubKey.size()
	);
	Utils::putVal<uint32_t>(tmp, challenge);
	if (ipType) {
		Utils::putVal<uint32_t>(tmp, ip);
		Utils::putVal<uint8_t>(tmp, ipType);
	}
	std::string msg(tmp.str());

	const uint8_t *msgPtr = reinterpret_cast<const uint8_t*>(msg.c_str());
	const uint8_t *sigPtr = reinterpret_cast<const uint8_t*>(sign.c_str());
	return verifier.VerifyMessage(msgPtr, msg.size(), sigPtr, sign.size());
}

Credits* CreditsDb::create(PublicKey key, const Hash<MD4Hash> &hash) {
	KeyIter it = m_list->get<ID_PubKey>().find(key);
	if (it != m_list->get<ID_PubKey>().end()) {
		return *it;
	} else {
		Credits *c = new Credits(key, hash);
		c->setLastSeen(Utils::getTick() / 1000);
		m_list->insert(c);
		return c;
	}
}

} // end namespace Donkey
