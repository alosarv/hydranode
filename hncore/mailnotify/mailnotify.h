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
 * \file mailnotify.h Interface for email notifier plugin
 */

#ifndef __MAILNOTIFY_H__

#include <hncore/modules.h>
#include <hnbase/ssocket.h>

class PartData;
class SharedFile;

namespace MNotify {

class MailNotify;
typedef SSocket<MailNotify, Socket::Client, Socket::TCP> MailSocket;

/**
 * MailNotify plugin sends e-mail on download completition (and in the future,
 * also on custom events). The following configuration values must be present
 * for this to work:
 * [MailNotify]
 * Server=ip_of_the_server
 * From=senders@mail.addr
 * To=where@to.send.it
 */
class MailNotify : public ModuleBase {
	DECLARE_MODULE(MailNotify, "mailnotify");
public:
	//! Called by core when module is being loaded
	bool onInit();

	//! Called by core when module is being unloaded
	int onExit();

	std::string getDesc() const { return "e-mail notifications support"; }
private:
	/**
	 * Handles SF_DL_COMPLETED event and sends email
	 *
	 * @param sf     SharedFile that generated the event
	 * @param evt    Event which was emitted
	 */
	void onSFEvent(SharedFile *sf, int evt);

	/**
	 * Socket event handler
	 *
	 * @param sock   Socket that generated the event
	 * @param evt    Event that was generated
	 */
	void onSocketEvent(MailSocket *sock, SocketEvent evt);

	/**
	 * Send an email notification that a file has been completed
	 *
	 * @param sf     File that was completed
	 *
	 * \throws if m_socket is null or isn't connected
	 */
	void sendNotify(SharedFile *sf);

	/**
	 * Moves settings in Prefs from MailNotify to mailnotify section.
	 */
	void migrateSettings();

	std::string m_mailServer;    //!< URL/IP address of mail server
	std::string m_mailFrom;      //!< Mail sender's address
  	std::string m_mailTo;        //!< Where to send the mail

	//! Pending completed downloads
	std::vector<SharedFile*> m_pendingMessages;

	//! Socket used for mailserver communication
	MailSocket *m_socket;
};

}

#endif

