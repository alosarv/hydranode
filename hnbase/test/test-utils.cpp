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

#include <hnbase/utils.h>
void logMsg(const boost::format &msg) {
	std::cerr << msg << std::endl;
}

using namespace Utils;
static const uint32_t testCnt = 1000000;
int main() {
	logMsg(
		boost::format("Testing Utils IO methods speed with %d testCnt.")
		% testCnt
	);

	{
		std::ostringstream tmp;
		StopWatch s;
		for (uint32_t i = 0; i < testCnt; ++i) {
			putVal<uint8_t>(tmp, 100);
		}
		logMsg(boost::format("Writing uint8_t:    %5dms") % s.elapsed());

		std::istringstream tmp2(tmp.str());
		uint8_t retVal;
		StopWatch s2;
		for (uint32_t i = 0; i < testCnt; ++i) {
			retVal = getVal<uint8_t>(tmp2);
		}
		logMsg(boost::format("Reading uint8_t:    %5dms") %s2.elapsed());
	}
	{
		std::ostringstream tmp;
		StopWatch s;
		for (uint32_t i = 0; i < testCnt; ++i) {
			putVal<uint16_t>(tmp, 1000000);
		}
		logMsg(boost::format("Writing uint16_t:   %5dms") % s.elapsed());

		std::istringstream tmp2(tmp.str());
		uint16_t retVal;
		StopWatch s2;
		for (uint32_t i = 0; i < testCnt; ++i) {
			retVal = getVal<uint16_t>(tmp2);
		}
		logMsg(boost::format("Reading uint16_t:   %5dms") %s2.elapsed());
	}
	{
		std::ostringstream tmp;
		StopWatch s;
		for (uint32_t i = 0; i < testCnt; ++i) {
			putVal<uint32_t>(tmp, 100000000);
		}
		logMsg(boost::format("Writing uint32_t:   %5dms") % s.elapsed());

		uint32_t retVal;
		std::istringstream tmp2(tmp.str());
		StopWatch s2;
		for (uint32_t i = 0; i < testCnt; ++i) {
			retVal = getVal<uint32_t>(tmp2);
		}
		logMsg(boost::format("Reading uint32_t:   %5dms") %s2.elapsed());
	}
	{
		std::ostringstream tmp;
		StopWatch s;
		for (uint32_t i = 0; i < testCnt; ++i) {
			putVal<std::string>(tmp, "Hello world", 11);
		}
		logMsg(boost::format("Writing string:     %5dms") % s.elapsed());

		std::string retVal;
		std::istringstream tmp2(tmp.str());
		StopWatch s2;
		for (uint32_t i = 0; i < testCnt; ++i) {
			retVal = getVal<std::string>(tmp2, 11);
		}
		logMsg(boost::format("Reading string:     %5dms") %s2.elapsed());
	}
	{
		std::ostringstream tmp;
		StopWatch s;
		for (uint32_t i = 0; i < testCnt; ++i) {
			putVal<std::string>(tmp, "Hello world");
		}
		logMsg(boost::format("Writing string+len: %5dms") % s.elapsed());

		std::string retVal;
		std::istringstream tmp2(tmp.str());
		StopWatch s2;
		for (uint32_t i = 0; i < testCnt; ++i) {
			retVal = getVal<std::string>(tmp2);
		}
		logMsg(boost::format("Reading string+len: %5dms") %s2.elapsed());
	}
	{
		std::string longString(10000, 'a');
		std::ostringstream tmp;
		StopWatch s;
		for (uint32_t i = 0; i < testCnt/100; ++i) {
			putVal<std::string>(tmp, longString, 10000);
		}
		logMsg(boost::format("Writing longString: %5dms") % s.elapsed());

		std::istringstream tmp2(tmp.str());
		std::string retVal;
		StopWatch s2;
		for (uint32_t i = 0; i < testCnt/100; ++i) {
			retVal = getVal<std::string>(tmp2, 10000);
		}
		logMsg(boost::format("Reading longString: %5dms") %s2.elapsed());
	}
	return 0;
}
