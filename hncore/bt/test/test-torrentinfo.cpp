/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#include <hncore/bt/torrentinfo.h>
#include <fstream>
using namespace Bt;

int main(int argc, char *argv[]) {
	assert(argc == 2);
	std::ifstream f(argv[1], std::ios::binary);
	std::string data;
	char buf[10240];
	while (f) {
		f.read(buf, 10240);
		data.append(std::string(buf, f.gcount()));
	}
//	std::cerr << Utils::hexDump(data) << std::endl;
	TorrentInfo ti(data);
	ti.print();

	f.close();
	f.open(argv[1], std::ios::binary);
	TorrentInfo ti2(f);
	ti2.print();

	assert(ti == ti2);

	return 0;
}
