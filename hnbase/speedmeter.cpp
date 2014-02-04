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
 * \file speedmeter.cpp      Implementation of SpeedMeter class
 */

#include <hnbase/speedmeter.h>

/*
 * Original idea and implementation credits go to Xaignar.
 *
 * There are two key variables in this speedmeter implementation - history size,
 * and resolution.
 *
 * Resolution defines how large is the allowed error, e.g. in case of 100ms
 * resolution, getSpeed() accuracy is 100ms in best-case scenario.
 *
 * History size defines how large amount of history data should be gathered.
 * The getSpeed() returns the sum of all history values, plus the m_recent
 * value, which keeps the last (not-yet-in-history) value. This results in
 * possible fluctuation of speeds at most @resolution amount.
 *
 * Gaps longer than histSize - 1 would result in a queue of zeroes, so to avoid
 * needless list manipulation, we can simply clear the list if the gap becomes
 * larger than histSize - 1 (which gives the same result, e.g. 0).
 *
 * If the gap is more than a single @resolution interval, then the first
 * @resolution interval will contain the last "recent" value, whereas the next
 * intervals will be zero, as no adds occoured for them.
 *
 * Total of histSize - 1 entries are kept, which together with the amount stored
 * in m_recent give a timespan of histSize * resolution.
 */

const uint64_t *SpeedMeter::m_ticker = 0;

SpeedMeter::SpeedMeter() : m_recent(), m_current(), m_total(), m_lastReset(),
m_histSize(10), m_res(100) {
}

SpeedMeter::SpeedMeter(uint32_t histSize, uint32_t resolution) : m_recent(),
m_current(), m_total(), m_lastReset(), m_histSize(histSize), m_res(resolution) {
}

SpeedMeter& SpeedMeter::operator+=(uint32_t amount) {
	uint64_t curTick = m_ticker ? *m_ticker : Utils::getTick();

	if (m_lastReset + (m_histSize - 1) * m_res < curTick) {
		m_history.clear();
		m_recent     = amount;
		m_lastReset += m_res * ((curTick - m_lastReset) / m_res);
		m_current    = amount;
	} else if (m_lastReset + m_res < curTick) {
		do {
			m_history.push_back(m_recent);
			m_lastReset += m_res;
			m_recent     = 0;
		} while (m_lastReset + m_res < curTick);
		m_recent   = amount;
		m_current += amount;
		while (m_history.size() > m_histSize - 1) {
			m_current -= m_history.front();
			m_history.pop_front();
		}
	} else {
		m_recent  += amount;
		m_current += amount;
	}
	m_total += amount;
	return *this;
}

void SpeedMeter::setTicker(const uint64_t &ticker) {
	m_ticker = &ticker;
}

void SpeedMeter::unsetTicker() {
	m_ticker = 0;
}
