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

#ifndef __SUB_CONFIG_H__
#define __SUB_CONFIG_H__

#include <hncore/cgcomm/subsysbase.h>

namespace CGComm {
namespace Subsystem {

/**
 * Config subsystem implements support for viewing, updating and receiving
 * notifications about changes to application settings; it is an interface for
 * Prefs::instance().
 */
class Config : public SubSysBase {
public:
	Config(boost::function<void (const std::string&)> sendFunc);

	//! Handles incoming packets
	virtual void handle(std::istream &i);
private:
	// Handles OC_GET message
	void getValue(std::istream &i);

	//! Handles OC_SET message
	void setValue(std::istream &i);

	//! Handles OC_LIST message
	void getList();

	//! Handles OC_MONITOR message
	void monitor();

	/**
	 * Signal handler for configuration value changes; sends the change
	 * to UI if m_monitor is set true.
	 */
	void valueChanged(const std::string &key, const std::string &value);

	/**
	 * Sends value change notification to user interface, either in response
	 * to OC_GET message, or when monitor is enabled and valueChanged signal
	 * is received.
	 *
	 * @param key        Full path of the key
	 * @param value      The current value of the key
	 */
	void sendValue(const std::string &key, const std::string &value);

	/**
	 * If true, valueChanged signal handler will send the changed value to
	 * user interface.
	 */
	bool m_monitor;
};

}
}

#endif
