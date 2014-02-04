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
 * \file config.cpp Implementation of Config class.
 */

#include <hnbase/pch.h>
#include <hnbase/config.h>
#include <hnbase/log.h>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string/trim.hpp>

// constructors/destructors
Config::Config() : m_curPath("/") {}
Config::Config(const std::string &filename) : m_curPath("/") {

	// initialize signals from main app
	valueChanging("", "");
	valueChanged("", "");

	load(filename);
}

Config::~Config() {
}

/**
 * Loads data from configuration file.
 */
void Config::load(const std::string &filename) {
	CHECK_THROW(filename.size());

	m_configFile = filename;
	setPath("/");
	std::ifstream ifs(filename.c_str(), std::ios::in);
	if (!ifs) {
		logWarning(
			"No config file found, using default configuration "
			"values."
		);
		return;
	}

	while (ifs.good()) {
		std::string line;
		std::getline(ifs, line);

		// Remove preceeding whitespace
		boost::algorithm::trim(line);
		if (line.length() < 2) {
			continue;
		}

		// Directories
		if ((*(line.begin()) == '[') && (*(--line.end()) == ']')) {
			std::string dir;
			dir += line.substr(1, line.length() - 2);
			setPath("/" + dir);

			logTrace(TRACE_CONFIG,
				boost::format("Reading: got path=%s") % dir
			);

			continue;
		}

		// Values
		size_t pos = line.find('=', 0);
		if (pos != std::string::npos) {
			std::string name = line.substr(0, pos);
			std::string value = line.substr(pos + 1);
			m_values[m_curPath + name] = value;

			logTrace(TRACE_CONFIG,
				boost::format(
					"Reading: got key=%s "
					"value=%s curpath=%s"
				) % name % value % m_curPath
			);

			continue;
		}
	}
	setPath("/");
}

/**
 * Saves data into configuration file.
 */
void Config::save(const std::string &filename) const {
	CHECK_THROW(filename.size());
	std::ofstream ofs(filename.c_str(), std::ios::out);
	if (!ofs) {
		logError(
			boost::format("Failed to open config file '%s'"
			" for writing.") % filename
		);
		return;
	}

	// root dir values copied here and written separately
	std::vector<std::pair<std::string, std::string> > rootValues;

	std::string curpath = "/";
	std::string wrpath = "";
	for (CIter i = m_values.begin(); i != m_values.end(); ++i) {
		size_t pos = (*i).first.find_last_of('/');
		if (pos == 0) {
			rootValues.push_back(*i);
			continue;
		}
		std::string key = (*i).first.substr(pos + 1);
		std::string path = (*i).first.substr(1, pos > 0 ? pos - 1 :pos);
		std::string value = (*i).second;

		logTrace(TRACE_CONFIG,
			boost::format("Writing: Key=%s Value=%s Path=%s")
			% key % value % path
		);

		if (path != wrpath) { // Path not written yet
			ofs << "[" << path << "]\n";
			wrpath = path;
		}
		ofs << key << "=" << value << std::endl;
	}

	// root keys are written in section []
	ofs << "[]" << std::endl;
	while (rootValues.size()) {
		ofs << rootValues.back().first.substr(1) << "=";
		ofs << rootValues.back().second << std::endl;
		rootValues.pop_back();
	}
}

/**
 * Changes current active directory.
 */
void Config::setPath(const std::string &dir) {
	CHECK_THROW(dir.size());

	if (*(dir.begin()) == '/') {
		m_curPath = dir;
	} else {
		m_curPath += dir;
	}
	if (*--m_curPath.end() != '/') {
		m_curPath += '/';
	}
}

void Config::dump() const {
	boost::format fmt("[%s]=%s");
	for (CIter i = m_values.begin(); i != m_values.end(); ++i) {
		logTrace(TRACE_CONFIG, fmt % (*i).first % (*i).second);
	}
}
