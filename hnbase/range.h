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
 * \file range.h Interface for Range class
 */

#ifndef __RANGE_H__
#define __RANGE_H__

#include <hnbase/osdep.h>
#include <hnbase/utils.h>
#include <boost/logic/tribool.hpp>
#include <boost/concept_check.hpp>
#include <set>
#include <limits>

namespace CGComm {
	enum {
		OP_RANGE = 0x10
	};
}

/**
 * Range object represents a range of values. Range object always has a begin
 * and an end (inclusive). Ranges can be compared, merged, and split. The
 * underlying object type used for representing the begin/end values of the
 * range are generally required to be of integer type, but any suitable
 * user-defined type can also be used that satisfies the comparison operations
 * used by this class.
 */
template<typename T>
class Range {
public:
	typedef T size_type;
	BOOST_CLASS_REQUIRE(size_type, Utils, IntegerConcept);

	//! @name Construction
	//! @{
	Range(const size_type &begin, const size_type &end)
	: m_begin(begin), m_end(end) {
		CHECK_THROW(m_begin <= m_end);
	}
	template<typename X>
	Range(const Range<X> &x) {
		m_begin = x.m_begin;
		m_end = x.m_end;
	}
	Range(const std::pair<size_type, size_type> r)
	: m_begin(r.first), m_end(r.second) {
		CHECK_THROW(m_begin <= m_end);
	}
	Range(std::istream &i) {
		m_begin = Utils::getVal<T>(i);
		m_end   = Utils::getVal<T>(i);
		CHECK_THROW(m_end >= m_begin);
	}
	template<typename X>
	Range& operator=(const Range<X> &x) {
		m_begin = x.m_begin;
		m_end = x.m_end;
	}
	//! Constructs lenght=1 range
	explicit Range(const size_type &t) : m_begin(t), m_end(t) {}
	//!@}

	//! @name Comparing ranges
	//! @{
	template<typename X>
	bool operator<(const Range<X> &x) const {
		size_type avgThis = m_begin + ((m_end - m_begin) / 2);
		size_type avgThat = x.m_begin + ((x.m_end - x.m_begin) / 2);
		return avgThis < avgThat;
	}
	template<typename X>
	bool operator>(const Range<X> &x) const {
		size_type avgThis = m_begin + ((m_end - m_begin) / 2);
		size_type avgThat = x.m_begin + ((x.m_end - x.m_begin) / 2);
		return avgThis > avgThat;
	}
	template<typename X>
	bool operator==(const Range<X> &x) const {
		return m_begin == x.m_begin && m_end == x.m_end;
	}
	template<typename X>
	bool operator!=(const Range<X> &x) const {
		return !(m_begin == x.m_begin && m_end == x.m_end);
	}
	friend std::ostream& operator<<(std::ostream &o, const Range &r) {
		Utils::putVal<uint8_t>(o, CGComm::OP_RANGE);
		Utils::putVal<uint16_t>(o, sizeof(T)*2);
		Utils::putVal<T>(o, r.m_begin);
		Utils::putVal<T>(o, r.m_end);
		return o;
	}
	//!@}

	//! @name Accessors
	//! @{
	T begin()  const { return m_begin;             }
	T end()    const { return m_end;               }
	T length() const { return m_end - m_begin + 1; }
	void begin(const T &b) {
		CHECK_THROW(b <= m_end);
		m_begin = b;
	}
	void end(const T &e) {
		CHECK_THROW(e >= m_begin);
		m_end = e;
	}
	//! @}

	//! @name Operations
	//! @{

	/**
	 * Merge another range with current one.
	 *
	 * @param x    Range to merge into current one.
	 *
	 * \note Implies that *this contains() @param x. Does nothing if that
	 *       is not true.
	 */
	template<typename X>
	void merge(const Range<X> &x) {
		if (m_begin > x.m_begin) {
			m_begin = x.m_begin;
		}
		if (m_end < x.m_end) {
			m_end = x.m_end;
		}
		if (m_end + 1 == x.m_begin) {
			m_end = x.m_end;
		}
		if (m_begin - 1 == x.m_end) {
			m_begin = x.m_begin;
		}
	}

	/**
	 * Erase target range from current one.
	 *
	 * @param x    Pointer to range to be erased. If range splitting
	 *             happened, it will be modified to contain the second half
	 *             of the resulting two ranges.
	 * @return     True if *this was truncated. False if this range was
	 *             completely cleared out by @param x. boost::indeterminate
	 *             if *this was split into two, and @param x contains the
	 *             second half.
	 */
	template<typename X>
	boost::logic::tribool erase(Range<X> *x) {
		if (x->m_begin <= m_begin && x->m_end >= m_end) {
			return false;
		}
		if (m_begin >= x->m_begin) {
			m_begin = x->m_end + 1;
			return true;
		} else if (m_end <= x->m_end) {
			m_end = x->m_begin - 1;
			return true;
		}
		if (contains(x->m_begin) && contains(x->m_end)) {
			X tmp = m_end;
			m_end = x->m_begin - 1;
			x->m_begin = x->m_end + 1;
			x->m_end = tmp;
			return boost::indeterminate;
		}
		return true;
	}

	/**
	 * Check if *this contains @param x. A range is considered to contain
	 * another range if it even partially overlaps with another range. To
	 * check if another range is completely contained within current range,
	 * use containsFull() method.
	 *
	 * @param x      Range to be checked against current range.
	 * @return       True if containment is true, false otherwise.
	 */
	template<typename X>
	bool contains(const Range<X> &x) const {
		if (contains(x.m_begin)) {
			return true;
		} else if (contains(x.m_end)) {
			return true;
		} else if (x.containsFull(*this)) {
			return true;
		}
		return false;
	}

	/**
	 * Simplified version of the above, this method checks if a given value
	 * exists in this range.
	 *
	 * @param x         Value to check
	 * @return          True if the value is within this range
	 */
	bool contains(const size_type &x) const {
		return x >= m_begin && x <= m_end;
	}

	/**
	 * Check if *this completely contains @param x
	 *
	 * @param x    Range to be checked against current range
	 * @return     True if @param x is completely within *this.
	 */
	template<typename X>
	bool containsFull(const Range<X> &x) const {
		if (m_begin <= x.m_begin && m_end >= x.m_end) {
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Check if @param x borders with *this. Bordering is defined if @param
	 * x begins right after *this's end, or *this's begin starts right after
	 * @param x's end.
	 *
	 * @param x      Range to be checked against bordering
	 * @return       True if @param x borders with *this.
	 */
	template<typename X>
	bool borders(const Range<X> &x) const {
		if (m_begin - 1 == x.m_end) {
			return true;
		}
		if (m_end + 1 == x.m_begin) {
			return true;
		}
		return false;
	}

	//! @}    !operations

	//! @name Convenience methods for easier usage
	//! @{
	bool contains(const size_type &x, const size_type &y) const {
		return contains(Range(x, y));
	}
	bool containsFull(const size_type &x, const size_type &y) const {
		return containsFull(Range(x, y));
	}
	//!@}
private:
	//! Default constructor behaviour cannot be defined and is forbidden
	Range();

	T m_begin;   //! begin offset
	T m_end;     //! end offset

	//! Be-friend with all our instances
	template<typename X> friend class Range;
};

//! \name Commonly used Range types
//! @{
typedef Range<uint64_t> Range64;
typedef Range<uint32_t> Range32;
typedef Range<uint16_t> Range16;
typedef Range<uint8_t>  Range8;
//! @}

#endif
