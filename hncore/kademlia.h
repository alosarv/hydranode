/*
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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
 * \file kademlia.h Generic Kademlia Implementation
 */

#ifndef __KADEMLIA_H__
#define __KADEMLIA_H__

#include <list>
#include <bitset>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <iterator>

#include <hnbase/config.h>
#include <hnbase/hash.h>
#include <hnbase/ipv4addr.h>
#include <hncore/modules.h>
#include <hncore/kademlia_util.h>

#include <boost/static_assert.hpp>

/**
 * @page kad Generic Kademlia implementation, version 2
 *
 * \section intro Introduction
 * Since Kademlia has been implemented in many different ways, this header 
 * supplies a generic templatized version that allow user to gain some code
 * reuse.
 *
 * \section types Types
 * An Id an unique identification number that identifies both nodes and values. 
 * Kademlia sheets assume a 160bit wide id, but some implementations use
 * a different number of bits: in this header, an Id is assumed to be a
 * std::bitset with the correct number of required bit. On top of Id, a
 * Kad::Contact is built: a Contact is a tuple containing a node Id, its ip
 * address and udp port. Kad::Contact is a template<int N>, where N is the
 * number of bits to use for id. Kad::Contact::IdType is defined as bitset<N>.
 * Implementations can inherit Kad::Contact to add data to each contact.
 *
 */

/**
 * Generic Kademlia implementation
 */
namespace Kad {

/**
 * Generic messages
 */
class Messages {
	enum __unnamed__ {
		PING, STORE, FIND_NODE, FIND_VALUE
	};
};

/**
 * Given a IdType, returns its prefix type in Typein Type. 
 * Specializing this template to use custom prefixes
 */
template<typename IdType>
struct Prefix {
	typedef std::pair<IdType, unsigned> Type;
};

/**
 * Assure bits after mask are set to 0
 */
template<typename IdType>
void prefixCheck(const typename Kad::Prefix<IdType>::Type& p) {
	IdType t(
		Kad::KUtils::buildMask(
			p.second, 
			Kad::KUtils::TypeToType<IdType>()
		) & p.first
	);

	CHECK_THROW(t == p.first);
}

/**
 * Compare two prefixes
 */
template<
	typename IdType
>
struct PrefixLess {
	typedef typename Prefix<IdType>::Type PrefixType;

	bool operator()(
		const typename Kad::Prefix<IdType>::Type& t, 
		const typename Kad::Prefix<IdType>::Type& u
	) const {
		prefixCheck<IdType>(t);
		prefixCheck<IdType>(u);

		return (t < u) || (t.first < u.first);
	}
};

/**
 * IdBelongsToPrefix predicate checks if an id belongs to a 
 * given prefix.
 *
 * @param IdType Type of id object
 */
template<
	typename IdType
>
struct IdBelongsToPrefix {
	typedef typename Prefix<IdType>::Type PrefixType;

	typedef bool result_type;

	bool operator()(const IdType& id) const {
		return (id & m_mask) == m_prefix;
	}

	IdBelongsToPrefix(const PrefixType& prefix)
	: m_mask(KUtils::buildMask(
		prefix.second, 
		KUtils::TypeToType<IdType>()
	)), m_prefix(prefix.first & m_mask)
	{ }

private:
	IdType m_mask;
	IdType m_prefix;
};

/**
 * PrefixContainsId predicate checks if a predicate contains a
 * given id.
 *
 * @param IdType Type of id object
 */
template<
	typename IdType
>
struct PrefixContainsId {
	typedef typename Prefix<IdType>::Type PrefixType;

	typedef bool result_type;

	bool operator()(const PrefixType &prefix) const {
		IdType mask(KUtils::buildMask(
			prefix.second, KUtils::TypeToType<IdType>()
		));

		return (m_id & mask) == (prefix.first & mask);
	}

	PrefixContainsId(const IdType &id)
	: m_id(id)
	{ }

private:
	IdType m_id;
};

/**
 * Kademlia standard contact
 *
 * @param N     The number of bit used per key/id. Defaults to 160,
 *              as described in kademlia draft.
 */
template<unsigned N = 160>
struct Contact {
	//! Number of bits per id
	enum { BITS = N };

	//! Declare IdType
	typedef std::bitset<N> IdType;

	//! Declare PrefixType
	typedef std::pair<IdType, unsigned> PrefixType;

	//! Contact id
	IdType          m_id;

	//! Contact address
	IPV4Address     m_addr;

	//! Is implicitly casted to id if needed
	operator const IdType&() const {
		return m_id;
	}

	//! Equality test
	bool operator==(const Contact& t) const {
		return m_id == t.m_id && m_addr == t.m_addr;
	}

	//! Operator< overload for ordering
	bool operator<(const Contact& contact) const {
		return m_id < contact.m_id;
	}

	//! Constructor
	Contact()
	: m_id(0), m_addr()
	{ }
};

/**
 * K-Bucket standard implementation
 *
 * @param ContactType   Contact type to be used
 * @param K             Maximum number of entry for this bucket, 
 *                      defaults to 20
 */
template<typename ContactType_, unsigned K = 20>
class KBucket {
public:
	//! Throw'd when KBucket is full
	struct Full : public std::runtime_error { 
		Full() : std::runtime_error("KBucket is full") { };
	};

	//! Contact type
	typedef ContactType_                   ContactType;
	typedef typename ContactType::IdType   IdType;

	//! Iterator type
	typedef typename std::list<ContactType>::iterator iterator;

	struct PrefixContainsId {
		typedef bool result_type;

		bool operator()(const KBucket &kbucket) const {
			const PrefixType &prefix(kbucket.m_prefix);

			IdType mask(KUtils::buildMask(
				prefix.second, KUtils::TypeToType<IdType>()
			));

			return (m_id & mask) == (prefix.first & mask);
		}

		PrefixContainsId(const IdType &id)
		: m_id(id)
		{ }

	private:
		IdType m_id;
	};

	//! Begin iterator
	iterator begin() {
		return entries.begin();
	}

	//! End iterator
	iterator end() {
		return entries.end();
	}

	//! Back inserter iterator
	std::back_insert_iterator<
		std::list<ContactType>
	> iitor() {
		return std::back_inserter(entries);
	}

	/**
	 * Returns kbucket size
	 */
	unsigned size() const {
		return entries.size();
	}

	/**
	 * Returns true if kbucket is full
	 */
	bool isFull() const {
		return entries.size() == K;
	}

	/**
	 * Add a contact
	 *
	 * @param contact   Contact to be added
	 * @param setAlive  If contact is already present in kbucket, 
	 *                  move it to tail (means that contact is
	 *                  alive)
	 */
	void add(const ContactType& contact, bool setAlive = false) {
		iterator i = std::find(entries.begin(), entries.end(), contact);

		if(i == end()) {
			if(isFull()) {
				throw Full();
			}

			logDebug(boost::format("KBucket is adding %s")
				% contact.m_id
			);

			entries.push_back(contact);
		} else if(setAlive) {
			// contact is already known, but since setAlive 
			// flag is true, set it on the tail

			std::iter_swap(--end(), i);
		}
	}

	/**
	 * Remove a contact
	 */
	void remove(const ContactType &contact) {
		iterator i = entries.find(contact);

		if(i != end()) {
			entries.erase(i);
		}
	}

	/**
	 * Constructor
	 */
	KBucket()
	{ }

	/**
	 * Copy constructor
	 */
	KBucket(const KBucket& copy)
	: entries(copy.entries)
	{ }

private:
	enum {
		NORMAL,
		WAITING_PING_REPLY
	} m_status;

	typedef typename Prefix<typename ContactType::IdType>::Type PrefixType;

	//! Bucket entries
	std::list<ContactType>          entries; // FIXME, should be m_entries

	//! This bucket prefix
	PrefixType                      m_prefix;

#ifdef FOR_FUTURE_USE
	ContactType                     m_pending;

	//! Process and entry
	void process(const ContactType &entry) {
		//! A predicate to lookup entries with id
		struct HasId {
		private:
			const Impl::Id &m_id;

		public:
			bool operator()(const Impl::Entry &entry) const {
				return entry.getId() == m_id;
			}

			//! Constructor
			HasId(id) : m_id(id) { }
		}; 

		// FIXMEEEEEEEEEEE FIXME
		typename std::list<typename Impl::Entry>::iterator itor;

		if(itor = std::find(
			entries.begin(), 
			entries.end(),
			entry
			//HasId(entry.getId())
		)) {
			// Known id, move to tail
			std::iter_swap(itor, --entries.end());
		} else if(entries.size() < K) {
			// Not found with free space
			entries.push_back(entry);
		} else if(m_status == NORMAL) {
			// Not found without free space and no pending entry
			m_pending = entry;
			m_status = WAITING_PING_REPLY;

			pingFirst();
		} else if(m_status = WAITING_PING_REPLY) {
			// Insert previous pending entry
			pingReply(m_pending, false);
		}
	}

	void pingReply(typename Impl::Entry &entry, bool result) {
		if(entry == m_pending) {
			if(!result) {
				// Remove first element
				entries.pop_front();

				// Add the pending element 
				entries.push_back(m_pending);

				// Set status back to normal
				m_status = NORMAL;
			} else {
				// First element answered, move it to tail and 
				ping 2nd
				std::iter_swap(
					entries.begin(), --entries.end()
				);

				pingFirst();
			}
		}
	}

	void pingFirst() {
		Impl::ping(
			boost::bind(&pingReply, this, entries.front(), _1)
		);
	}
#endif
};

/**
 * Given a std::pair, extract the first entry
 */
template<typename R>
struct ExtractFirst {
        typedef R result_type;

        template<typename T, typename U>
        result_type operator()(const std::pair<T, U>& t) const {
                return t.first;
        }
};

/**
 * Functor that derefences an iterator
 */
template<
	typename T
>
struct DereferenceIterator {
	typedef T result_type;

	template<class I>
	T operator()(const I &itor) {
		return *itor;
	}
};

/**
 * Returns the first id of a given prefix
 */
template<
	typename IdType
>
IdType prefixBase(const typename Prefix<IdType>::Type &prefix) {
	return prefix.first & KUtils::buildMask(
		prefix.second, 
		KUtils::TypeToType<IdType>()
	);
}

/**
 * Functor that compares two predicate using their distance from a given id.
 *
 * @param IdType Type of id object
 */
template<
	typename IdType
>
struct PrefixDistanceFromLess {
	typedef typename Prefix<IdType>::Type PrefixType;

	typedef bool result_type;

	bool operator()(const PrefixType &t, const PrefixType &u) const {
		return 
			(prefixBase<IdType>(t) | m_id) 
			< 
			(prefixBase<IdType>(u) | m_id);
	}

	PrefixDistanceFromLess(const IdType &id)
	: m_id(id)
	{ }

private:
	IdType m_id;
};

/**
 * RoutingTable
 *
 * @param KBucketType_ Used KBucket type
 * @param RPCFunctor   Callback for RPCs (currently not implemented)
 */
template<
	typename KBucketType_,
	typename RPCFunctor
>
class RoutingTable {
public:
	//! Public types

	//@{
	typedef KBucketType_                            KBucketType;
	typedef typename KBucketType::ContactType       ContactType;
	typedef typename ContactType::IdType            IdType;
	typedef typename Prefix<IdType>::Type           PrefixType;
	//@}

private:
	//! Private types

	//@{
	typedef std::map<PrefixType, KBucketType, PrefixLess<IdType> > RMap;
	//@}

public:
	//! Number of bits per id
	enum { BITS = ContactType::BITS };

	//! Dump (DEBUG)
	void dump() {
		typename RMap::iterator itor = m_tree.begin();
		typename KBucketType::iterator jtor;

		for(; itor != m_tree.end(); itor++) {
			logDebug(
				boost::format("%s/%i") 
				% KUtils::bitsetBitDump(itor->first.first) 
				% itor->first.second
			);

			for(
				jtor = itor->second.begin(); 
				jtor != itor->second.end(); 
				++jtor
			) {
				logDebug(boost::format("  %s") 
					% KUtils::bitsetBitDump(jtor->m_id)
				);
			}
		}
	}

	ContactType &getRandom() {
		static typename RMap::iterator itor = m_tree.begin();

		while(itor->second.size() == 0) {
			++itor;
		}

		return *(itor->second.begin());
	}

	/**
	 * Add a contact
	 *
	 * @param contact  The contact to be added.
	 * @param setAlive Sets the contact to alive (a packet has been received
	 *                 from there). Defaults to false.
	 */
	void add(const ContactType& contact, bool setAlive = false) {
		typename RMap::iterator itor;
	
		itor = std::find_if(m_tree.begin(), m_tree.end(), 
			boost::bind(
				PrefixContainsId<IdType>(
					contact.m_id ^ m_self.m_id
				), boost::bind(
					ExtractFirst<typename RMap::key_type>(),
					_1
				)
			)
		);

		CHECK_THROW(itor != m_tree.end());

		try {
			// Add contact to KBucket
			itor->second.add(contact, setAlive);
		} catch(typename KBucketType::Full &e) {
			// KBucket is full: split and retry
			split(itor);

			add(contact, setAlive);
		}
	}

	/**
	 * Remove a contact
	 */
	void remove(const ContactType &);

	/**
	 * Returns the prefix who contains a given id
	 *
	 * @param id  Id to be contained by returned prefix
	 */
	typename RMap::iterator getOwningPrefix(const IdType &id) {
		return std::find_if(m_tree.begin(), m_tree.end(),
			boost::bind(
				PrefixContainsId<IdType>(id),
				boost::bind(
					ExtractFirst<typename RMap::key_type>(),
					_1
				)
			)
		);
	}

	/** 
	 * Find closest ids to a given id.
	 *
	 * @param id  Id to be used while searching.
	 */
	std::list<ContactType> findClosestToId(const IdType &id) {
		typename RMap::iterator itor = getOwningPrefix(id);
		std::list<ContactType> list;

		CHECK_THROW(itor != m_tree.end());

		// Copy contacts from nearest kbucket
		std::copy(
			itor->second.begin(),
			itor->second.end(),
			std::back_inserter(list)
		);

		// Not enough contacts: find closest prefixes
		if(list.size() < 10) {
			typename RMap::iterator jtor(itor);
			std::vector<typename RMap::iterator> v;

			for(
				typename RMap::iterator zitor = m_tree.begin(); 
				zitor != m_tree.end(); 
				++zitor
			) {
				v.push_back(zitor);
			}


			if(jtor != m_tree.begin()) {
				do {
					v.push_back(--jtor);
				} while(jtor != m_tree.begin());
			}

			jtor++ = itor;
			
			while(jtor != m_tree.end()) {
				v.push_back(jtor++);
			}

			//  PrefixDistanceFromLess(
			//   ExtractFirst(DereferenceIterator(_1)), 
			//   ExtractFirst(DereferenceIterator(_2))
			//  )

			typedef typename RMap::key_type RMKey;
			typedef typename RMap::value_type RMValue;

			// Now order prefixes by distance
			std::sort(v.begin(), v.end(), 
				boost::bind(
					PrefixDistanceFromLess<IdType>(id),
					boost::bind(
						ExtractFirst<RMKey>(),
						boost::bind(
							DereferenceIterator<
								RMValue
							>(), _1
						)
					),
					boost::bind(
						ExtractFirst<RMKey>(),
						boost::bind(
							DereferenceIterator<
								RMValue
							>(), _2
						)
					)
				)
			);

			typename std::vector<
				typename RMap::iterator
			>::iterator kitor = v.begin();

			while(list.size() < 10 && kitor != v.end()) {
				// Copy contacts from nearest kbucket
				std::copy(
					(*kitor)->second.begin(),
					(*kitor)->second.end(),
					std::back_inserter(list)
				);

				kitor++;
			}
		}

		return list;
	}

	/**
	 * Constructor
	 *
	 * @param f   RPC callback functor
	 */
	RoutingTable(RPCFunctor f) 
	: m_rpcHandler(f) 
	{ 
		IdType id;
		id.reset();

		m_tree.insert(std::make_pair(
			std::make_pair(id, 0),
			KBucketType()
		));
	}

	//! Iterator forward declaration
	struct Iterator;
	typedef Iterator iterator;

	//! Our id
	ContactType     m_self;

private:
	//! Routing table
	RMap            m_tree;
		
	//! RPC Functor
	RPCFunctor      m_rpcHandler;

	static IdType xorFun(const IdType& t, const IdType& u) {
		return t ^ u;
	}

	void split(typename RMap::iterator itor) {
		PrefixType lprefix(itor->first), rprefix(itor->first);

		lprefix.first[itor->first.second] = 0;
		rprefix.first[itor->first.second] = 1;

		lprefix.second++;
		rprefix.second++;

		KBucketType lbucket, rbucket;

		std::remove_copy_if(
			itor->second.begin(), 
			itor->second.end(), 
			rbucket.iitor(),
			boost::bind(IdBelongsToPrefix<IdType>(lprefix),
				boost::bind(&xorFun, m_self.m_id, _1)
			)
		);

		std::set_difference(
			itor->second.begin(),
			itor->second.end(),
			rbucket.begin(),
			rbucket.end(),
			lbucket.iitor()
		);

		CHECK_THROW(
			rbucket.size() + lbucket.size() == itor->second.size()
		);

		m_tree.erase(itor);

		m_tree.insert(std::make_pair(lprefix, lbucket));
		m_tree.insert(std::make_pair(rprefix, rbucket));
	}

public:
	//! Iterator
	class Iterator
	: public std::iterator<
		std::forward_iterator_tag,
		ContactType,
		ptrdiff_t,
		ContactType *,
		ContactType &
	> {
		typename RMap::iterator         m_mIter;
		typename KBucketType::iterator  m_kIter;

		//! Iterator increment
		void increment() {
			typename RMap::iterator mNext(m_mIter);

			if(m_kIter == m_mIter->second.end()) {
				m_kIter = (++m_mIter)->second.begin();
			}

			m_kIter++;
		}

		//! Constructor
		Iterator(
			typename RMap::iterator mIter, 
			typename KBucketType::iterator kIter
		)
		: m_mIter(mIter), m_kIter(kIter)
		{ }

		friend class RoutingTable;
	public:
		//! Preincrement
		Iterator &operator++() {
			increment();

			return *this;
		}

		//! Postincrement
		Iterator operator++(int) {
			Iterator i(*this);
		
			increment();

			return i;
		}

		//! Dereference
		ContactType &operator*() {
			if(m_kIter == m_mIter->second.end()) {
				typename RMap::iterator mNext(m_mIter);

				return *(++mNext)->second.begin();
			}

			return *m_kIter;
		}

		//! Member access
		ContactType *operator->() { 
			if(m_kIter == m_mIter->second.end()) {
				typename RMap::iterator mNext(m_mIter);

				return &*(++mNext)->second.begin();
			}

			return &(*m_kIter);
		}

		//! Equality test
		bool operator==(const Iterator &t) const {
			return m_mIter == t.m_mIter && m_kIter == t.m_kIter;
		}

		//! Inequality test
		bool operator!=(const Iterator &t) const {
			return !(*this == t);
		}
	};

	Iterator begin() {
		return Iterator(m_tree.begin(), m_tree.begin()->second.begin());
	}

	Iterator end() {
		return Iterator(--m_tree.end(), (--m_tree.end())->second.end());
	}
};

}

namespace std {

/**
 * Compare two bitsets threading them as numbers (bit[0] is evaluated as LSB) 
 */
template<size_t N>
bool operator<(const std::bitset<N>& a, const std::bitset<N>& b) {
	for(int i = N - 1; i >= 0; --i) {
		if(a[i] < b[i])
			return true;
	}

	return false;
}

}

#endif
