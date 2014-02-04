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

#include <hnbase/speedmeter.h>
int main() {
	uint64_t curTick = Utils::getTick();
	SpeedMeter::setTicker(curTick);
	SpeedMeter speed(50, 100);
	Utils::StopWatch stopper;
	while (true) {
		speed += 100;
		if (stopper.elapsed() > 100) {
			curTick = Utils::getTick();
			std::cerr << '\r' <<
			Utils::bytesToString(speed.getSpeed(3000)/3) << "/s (";
			std::cerr << Utils::bytesToString(speed.getTotal());
			std::cerr << ")     ";
			stopper.reset();
		}
	}
}
