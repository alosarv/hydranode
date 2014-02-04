/*
 *  Copyright (C) 2005 Lorenz Bauer <scahoo@REMOVEgmail.com>
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

/*
 * This is a very simple template parser. It takes one template file as input
 * and transforms it into the output file by replacing certain placeholders
 * in the file with values specified on the commandline.
 * All output files will be chmod'ed +x on UNIX platforms, as this program is
 * intended for use with template shell scripts.
 *
 * Example:
 *   > wrapperc mytemplate.txt shellscript.sh "PREFIX=/usr/local"
 *   This will replace all occurences of {@PREFIX@} in mytemplate.txt with
 *   /usr/local and output the result to shellscript.sh. Note that you can
 *   specify as many placeholder/value pairs as you like.
 *
 * Syntax:
 *   wrapperc <template file> <output file> ( <placeholder>=<value> )*
 *
 * Compile:
 *   gcc -o wrapperc wrapperc.cpp -lstdc++
 */

#include <iostream>
#include <fstream>
#include <vector>

#include <hnbase/osdep.h>

#ifndef WIN32
	// Every POSIX'ish system should have these.
	#include <sys/types.h>
	#include <sys/stat.h>
#endif

using namespace std;

typedef vector< pair<string, string> > VarMap;
typedef VarMap::iterator VarIter;

int main(int argc, char** argv) {
	if (argc < 3) {
		cerr << "Invalid parameter count!" << endl;
		cerr << "Syntax:" << endl;
		cerr << "    wrapperc <template> <out-file> ( <varname>=";
			cerr << "<value> )*";
		cerr << endl;
		return 1;
	}

	ifstream infile(argv[1], ios::in);
	ofstream outfile(argv[2], ios::out | ios::trunc);

	if (!infile.is_open()) {
		cerr << "Couldn't open template '" << argv[1] << "'!" << endl;
		return 1;
	}

	if (!outfile.is_open()) {
		cerr << "Couldn't open outfile '" << argv[2] << "'!" << endl;
		return 1;
	}

	// Parse placeholder -> value pairs given via commandline.
	VarMap vars;
	for (int i = 3; i < argc; ++i) {
		string tmp(argv[i]);
		string::size_type pos = tmp.find("=", 0);
		if (pos != string::npos) {
			vars.push_back(make_pair(
				tmp.substr(0, pos),
				tmp.substr(pos + 1)
			));
		} else {
			cerr << "'" << tmp << "' isn't a valid ";
			cerr << "key -> value pair" << endl;
		}
	}

	// Transform the file.
	char buffer[512];
	while (!infile.eof()) {
		infile.getline(buffer, 512);

		string line(buffer);
		for (VarIter i = vars.begin(); i != vars.end(); ++i) {
			string::size_type pos = line.find("{@" + (*i).first + "@}", 0);
			if (pos != string::npos) {
				line.replace(
					pos,
					(*i).first.length() + 4,
					(*i).second
				);
			}
		}

		outfile << line << endl;
	}

	infile.close();
	outfile.close();

	// If you don't want the chmod behaviour on UNIX you'll have to delete
	// this.
	#ifndef WIN32
		int ret = chmod(argv[2],
			S_IRUSR | S_IWUSR | S_IXUSR |
			S_IRGRP | S_IXGRP |
			S_IROTH | S_IXOTH
		);

		if (ret == -1) {
			cerr << "Warning: couldn't set permissions on outfile!";
			cerr << endl;
		}
	#endif

	return 0;
}
