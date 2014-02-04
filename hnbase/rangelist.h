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
 * \file rangelist.h Interface for RangeList class
 */

#ifndef __RANGELIST_H__
#define __RANGELIST_H__

#include <hnbase/range.h>
#include <hnbase/lambda_placeholders.h>
#include <hnbase/utils.h>              // for IntegerConcept
#include <boost/concept_check.hpp>     // for concept checks

namespace CGComm {
	enum {
		OP_RANGELIST = 0x11
	};
}

/**
 * RangeList is a generic container for Range type objects. It supports
 * all the operations single Range objects support, however is also capable
 * of performing those operations over several ranges. RangeList provides
 * public typedef CIter for iterating on the underlying implementation.
 * Ranges in RangeList are guaranteed to be ordered based on Range begin values,
 * as defined by Range object.
 *
 * RangeList does not perform any lower or upper bound checking, except for
 * those inherent from the underlying data type.
 *
 * Ranges can be either push()'ed or merge()'d into RangeList. In the former
 * case, duplicates are allowed, and overlapping/bordering ranges are not merged
 * together into larger ranges. In the latter case, overlapping/bordering ranges
 * are compacted into bigger Range objects. The same logic applies to remove()
 * and erase() methods, former erasing an exact copy of the passed Range, latter
 * erasing a region of RangeList, possibly modifying existing ranges. After
 * call to erase(X, Y), it is guaranteed that RangeList::contains(X, Y) shall
 * return false, however in case of remove(), there is no such guarantee.
 *
 * All operations on RangeLists have logarithmic complexity, so it is possible
 * to store vast amounts of ranges here with little performance penalty.
 *
 * @param T         Type of ranges to be contained.
 *
 * The specified type must implement the following:
 *
 * \section Types Types
 * \code
 * typedef X size_type
 * \endcode
 *
 * \section Accessors/Queries Accessors/Queries
 * \code
 * X begin() const;
 * X end() const;
 * X length() const;
 * bool contains(const X &x) const;
 * bool containsFull(const X &x) const;
 * bool borders(const X &x) const;
 * \endcode
 *
 * \section Comparisons Comparison operators
 * \code
 * bool operator<(const T &x) const;
 * bool operator>(const T &x) const;
 * bool operator==(const T &x) const;
 * bool operator!=(const T &x) const;
 * \endcode
 *
 * \section Modifiers Modifiers
 * \endcode
 * void begin(const X &b);
 * void end(const X &b);
 * void merge(const X &x);
 * boost::logic::tribool erase(X *x);
 * \endcode
 */
template<typename RangeType>
class RangeList {
public:
	typedef std::multiset<RangeType>      Impl;
	typedef typename Impl::iterator       Iter;
	typedef typename Impl::const_iterator CIter;
	typedef typename RangeType::size_type size_type;

	BOOST_CLASS_REQUIRE(RangeType, boost, EqualityComparableConcept);
	BOOST_CLASS_REQUIRE(RangeType, boost, LessThanComparableConcept);
	BOOST_CLASS_REQUIRE(size_type, Utils, IntegerConcept);

	//! Default constructor
	RangeList() {}

	//! Destructor
	~RangeList() {}

	//! Construct and load from stream
	RangeList(std::istream &i) {
		using namespace Utils;
		uint32_t cnt = getVal<uint32_t>(i);
		for (uint32_t j = 0; j < cnt; ++j) {
			CHECK_THROW(getVal<uint8_t>(i) == CGComm::OP_RANGE);
			CHECK_THROW(getVal<uint16_t>(i) == sizeof(size_type)*2);
			m_ranges.insert(RangeType(i));
		}
	}

	//! Output operator for streams
	friend std::ostream& operator<<(std::ostream &o, const RangeList &rl) {
		Utils::putVal<uint8_t>(o, CGComm::OP_RANGELIST);
		Utils::putVal<uint16_t>(o, rl.size()*(sizeof(size_type)*2+3));
		Utils::putVal<uint32_t>(o, rl.size());
		for_each(rl.begin(), rl.end(), o << __1);
		return o;
	}

	/**
	 * Push a range into the rangelist.
	 *
	 * @param r       Range to be inserted
	 * @return        True if the range was inserted, false if it already
	 *                existed (e.g. duplicates are not allowed)
	 */
	bool push(const RangeType &r) {
		Iter i = m_ranges.find(r);
		if (i != end() && *i == r) {
			return false;
		}
		m_ranges.insert(r);
		return true;
	}

	/**
	 * Remove an exact range from the rangelist.
	 *
	 * @param r       Range to be removed
	 * @return        True if the exact range was found and removed, false
	 *                otherwise.
	 */
	bool remove(const RangeType &r) {
		Iter i = m_ranges.find(r);
		if (i != end() && *i == r) {
			m_ranges.erase(i);
			return true;
		}
		return false;
	}

	/**
	 * Merge a range into the existing rangelist. Merging concept means all
	 * ranges bordering or overlapping with the passed range will be
	 * "merged" together with the passed range, and erased from the list,
	 * after which the (now modified) passed range is inserted to the
	 * list.
	 *
	 * @param r       Range to be merged into the rangelist.
	 */
	void merge(RangeType r) {
		Iter i = doGetContains(r);
		while (i != end()) {
			r.merge(*i);
			m_ranges.erase(i);
			i = doGetContains(r);
		}
		i = doGetBorders(r);
		while (i != end()) {
			r.merge(*i);
			m_ranges.erase(i);
			i = doGetBorders(r);
		}
		m_ranges.insert(r);
	}

	/**
	 * Erase a range from the rangelist.
	 *
	 * @param r        Range to be erased
	 */
	void erase(const RangeType &r) {
		Iter i = doGetContains(r);
		while (i != m_ranges.end()) {
			RangeType one(*i), two(r);
			boost::logic::tribool ret = one.erase(&two);
			m_ranges.erase(i);
			if (ret == true) {
				m_ranges.insert(one);
			} else if (boost::indeterminate(ret)) {
				m_ranges.insert(one);
				m_ranges.insert(two);
			}
			i = doGetContains(r);
		}
	}

	/**
	 * Erases a specific iterator
	 *
	 * @param it       Iterator to be erased
	 */
	void erase(Iter it) {
		assert(it != m_ranges.end());
		m_ranges.erase(it);
	}

	/**
	 * Locate the first free (unused) range in the rangelist, optionally
	 * indicating the upper bound until which to search.
	 *
	 * @param limit        Optional upper bound for searching
	 * @return             An unused range
	 */
	RangeType getFirstFree(
		const size_type &limit = std::numeric_limits<size_type>::max()
	) const {
		size_type curPos = std::numeric_limits<size_type>::min();
		size_type endPos = std::numeric_limits<size_type>::max();
		if (size()) {
			if ((*begin()).begin() > curPos) {
				endPos = (*begin()).begin() - 1;
				if (endPos - curPos + 1 > limit) {
					endPos = curPos + limit - 1;
				}
			} else {
				curPos = (*begin()).end() + 1;
				if (++begin() == end()) {
					endPos = curPos + limit - 1;
				} else {
					endPos = (*++begin()).begin() - 1;
				}
			}
		} else {
			endPos = curPos + limit - 1;
		}
		return RangeType(curPos, endPos);
	}

	/**
	 * Check if a given range is contained within this rangelist.
	 *
	 * @param x        Range to be tested
	 * @return         True if the range is even partially contained within
	 *                 one of the existing ranges of this rangelist.
	 *
	 * \note Keep in mind that this method returns true even during partial
	 *       matches, e.g. when a subrange of @param x overlaps with one
	 *       (or more) of the existing ranges. True return value thus does
	 *       not indicate here that @param x is fully contained within this
	 *       rangelist. \see RangeList::containsFull()
	 */
	bool contains(const RangeType &r) const {
		return getContains(r) != end();
	}

	/**
	 * Check whether a given range is completely contained within this.
	 *
	 * @param x       Range to be tested
	 * @return        True if the range is fully contained in here.
	 */
	bool containsFull(const RangeType &r) const {
		CIter i = getContains(r);
		if (i != end() && (*i).containsFull(r)) {
			return true;
		} else if (i != begin() && (*--i).containsFull(r)) {
			return true;
		}
		return false;
	}

	/**
	 * Locate the first (?) range which contains the passed range.
	 *
	 * @param r       Range to be searched for
	 * @return        Iterator to the found range, or end() if not found.
	 *
	 * \note It is guaranteed that the returned range is the first range
	 *       (first in the sense of lower-value) range that contains the
	 *       questioned range. (hence the while-loop in here).
	 */
	CIter getContains(const RangeType &r) const {
		CIter i = m_ranges.lower_bound(r);
		if (i != end() && (*i).contains(r)) {
			while (i != begin() && (*--i).contains(r));
			return (*i).contains(r) ? i : ++i;
		} else if (i != begin() && (*--i).contains(r)) {
			return i;
		}
		return end();
	}

	/**
	 * Locate the first (?) range which borders with the passed range.
	 *
	 * @param r      Range to be searched for
	 * @return       Iterator to the found range, or end() if not found
	 */
	CIter getBorders(const RangeType &r) const {
		Iter i = m_ranges.lower_bound(r);
		if (i != end() && (*i).borders(r)) {
			return i;
		} else if (i != begin()) {
			if ((*--i).borders(r)) {
				return i;
			}
		} else if (i != end()) {
			if (++i != end() && (*i).borders(r)) {
				 return i;
			}
		}
		return end();
	}

	/**
	 * @name More convenient versions for the above functions
	 *
	 * These methods construct the RangeType object internally from the
	 * passed values. These only work when using built-in Range types, or
	 * user-defined types which do not pose additional constructor argument
	 * requirements.
	 */
	//! @{
	bool push(const size_type &x, const size_type &y) {
		return push(RangeType(x, y));
	}
	bool remove(const size_type &x, const size_type &y) {
		return remove(RangeType(x, y));
	}
	void merge(const size_type &x, const size_type &y) {
		merge(RangeType(x, y));
	}
	void erase(const size_type &x, const size_type &y) {
		erase(RangeType(x, y));
	}
	bool contains(const size_type &x, const size_type &y) const {
		return contains(RangeType(x, y));
	}
	bool containsFull(const size_type &x, const size_type &y) const {
		return containsFull(RangeType(x, y));
	}
	CIter getContains(const size_type &x, const size_type &y) const {
		return getContains(RangeType(x, y));
	}
	//! @}

	//! @name Generic accessors
	//! @{
	//! Returns number of elements in the rangelist
	size_t size() const { return m_ranges.size(); }
	//! Erases all elements in the list
	void clear() { m_ranges.clear(); }
	//! Tests if the rangelist is empty, e.g. size() == 0
	bool empty() const { return !size(); }
	//! @}

	/**
	 *  @name Accessors
	 *
	 *  \note The list is sorted based on the center-point of the ranges,
	 *        thus the first element in the list is not guaranteed to start
	 *        before every other element, however, it is guaranteed to have
	 *        the lowest begin+end/2 value.
	 */
	//! @{
	//! Constant iterator to the first element of the list
	CIter begin() const { return m_ranges.begin(); }
	//! Constant iterator to one-past-end of the list
	CIter end()   const { return m_ranges.end(); }
	//! Iterator to the first element of the list
	Iter  begin() { return m_ranges.begin(); }
	//! Iterator to one-past-end of the list
	Iter  end()   { return m_ranges.end();   }
	//! @}

	/**
	 * @name Accessors for first and last members of the list
	 *
	 * \return Reference to the element requested.
	 * \throws std::runtime_error if the list is empty.
	 */
	//! @{
	const RangeType& front() const {
		CHECK_THROW(size());
		return *begin();
	}
	const RangeType& back() const {
		CHECK_THROW(size());
		return *--end();
	}
	//! @}
private:
	Impl m_ranges;

	/**
	 * Implementation of getContains() method, using non-const types;
	 * Locates the first (?) range which contains the passed range.
	 *
	 * @param r    Range to be searched for
	 * @return     Iterator to the range, or end() if not found
	 */
	Iter doGetContains(const RangeType &r) {
		Iter i = m_ranges.lower_bound(r);
		if (i != end() && (*i).contains(r)) {
			while (i != begin() && (*--i).contains(r));
			return (*i).contains(r) ? i : ++i;
		} else if (i != begin() && (*--i).contains(r)) {
			return i;
		}
		return end();
	}

	/**
	 * Implementation of getBorders() method, using non-const types;
	 * Locates the first range which borders with the passed range.
	 *
	 * @param r       Range to be searched for
	 * @return        Iterator to the bordering range, or end() if not found
	 */
	Iter doGetBorders(const RangeType &r) {
		Iter i = m_ranges.lower_bound(r);
		if (i != end() && (*i).borders(r)) {
			return i;
		} else if (i != begin()) {
			if ((*--i).borders(r)) {
				return i;
			}
		} else if (i != end()) {
			if (++i != end() && (*i).borders(r)) {
				 return i;
			}
		}
		return end();
	}
};

//! \name Commonly used RangeList types
//! @{
typedef RangeList<Range64> RangeList64;
typedef RangeList<Range32> RangeList32;
typedef RangeList<Range16> RangeList16;
typedef RangeList<Range8 > RangeList8;
//! @}

#endif
