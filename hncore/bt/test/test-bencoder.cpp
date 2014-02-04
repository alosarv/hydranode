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

#include <hncore/bt/bencoder.h>
#include <iostream>

using namespace Bt;

// Sample usage - creates a standard .torrent file with example content
int main() {
	BDict torrent;
	torrent["announce"] = "http://tracker.prq.to/announce";
	torrent["creation date"] = 1115355915;

	BDict t_info; // "info" dictionary
	t_info["name"] = "My released torrent"; // top-most filename
	t_info["piece length"] = 250000;
	t_info["pieces"] = "asdfasdfagasdfasdfasdfasdfasdfasdfasdfasdfasdf"; // hashes

	BList files;

	BDict file1;
	file1["length"] = 123123;
	file1["path"] = "file1.avi";
	files.push_back(file1);

	BDict file2;
	file2["length"] = 123123;
	file2["name"] = "file2.avi";
	files.push_back(file2);

	t_info["files"] = files;
	torrent["info"] = t_info;

	std::cerr << torrent << std::endl;
}
