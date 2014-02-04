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

#ifndef __SPEEDMETER_H__
#define __SPEEDMETER_H__

/**
 * \file speedmeter.h   Interface for SpeedMeter class
 */

#include <hnbase/utils.h>
#include <hnbase/event.h>
#include <deque>

/**
 * SpeedMeter class provides a generic, customizable speed-calculation API.
 * It allows setting the history size, as well as resolution upon construction,
 * and provides accessor for average speed over a specified time, defaulting
 * to last 1000ms.
 */
class HNBASE_EXPORT SpeedMeter : public Trackable {
public:
	/**
	 * Default constructor, defaults to histSize 10 and 100ms resolution.
	 */
	SpeedMeter();

	/**
	 * Allows customizing history size and resolution.
	 *
	 * @param histSize      Number of values to keep in history
	 * @param resolution    Worst-case expected precision for getSpeed()
	 */
	SpeedMeter(uint32_t histSize, uint32_t resolution);

	/**
	 * Aquire the current speed.
	 *
	 * @param range        Time range for the requested speed, in
	 *                     milliseconds, e.g. 1000 for last seconds average,
	 *                     5000 for last 5 seconds average, et al. Note that
	 *                     if there's not enough history data for the
	 *                     calculation, it's "guessed" from known data.
	 * @returns            The current speed, in worst-case-precision of
	 *                     <b>resolution</b>.
	 *
	 * \note The const-cast is an implementation-artifact - in order to
	 *       aquire current speed, the internal structures must be refeshed,
	 *       however this method must remain const to the user.
	 */
	uint32_t getSpeed(float range = 1000.0) const {
		*const_cast<SpeedMeter*>(this) += 0;
		return static_cast<uint32_t>(
			m_current / (m_histSize * m_res / range)
		);
	}

	/**
	 * @name Generic accessors
	 */
	//!@{
	uint64_t getTotal()      const { return m_total;    }
	uint32_t getHistSize()   const { return m_histSize; }
	uint32_t getResolution() const { return m_res;      }
	//!@}

	/**
	 * Add data to the meter.
	 *
	 * @param amount       Amount to be added
	 * @returns            *this pointer
	 */
	SpeedMeter& operator+=(uint32_t amount);

	/**
	 * By default, SpeedMeter uses Utils::getTick() calls directly for time-
	 * calculations, but this may not be desired (for platforms where
	 * Utils::getTick() is expensive). Hence, this method allows setting a
	 * reference variable, which holds current tick value (probably set from
	 * application mainloop or similar), thus avoiding direct getTick()
	 * calls.
	 *
	 * \note SpeedMeter keeps a pointer to this value until unsetTicker()
	 *       is called. Make sure to call unsetTicker() before destroying
	 *       this variable.
	 */
	static void setTicker(const uint64_t &tick);

	/**
	 * Removes the external ticker variable.
	 */
	static void unsetTicker();
private:
	std::deque<uint32_t> m_history;    //!< History data
	uint32_t m_recent;                 //!< Most recent history value
	uint32_t m_current;                //!< Current speed
	uint64_t m_total;                  //!< Total data added
	uint64_t m_lastReset;              //!< Time of last history update

	uint32_t m_histSize;               //!< Size of history
	uint32_t m_res;                    //!< History values resolution

	static const uint64_t *m_ticker;   //!< Optional tick container
};

/**
 * Output operator to streams writes human-readable (converted to MB/s et al)
 * version of current speed, using default getSpeed() value.
 */
inline std::ostream& operator<<(std::ostream &o, const SpeedMeter &speed) {
	return o << Utils::bytesToString(speed.getSpeed());
}

#endif
