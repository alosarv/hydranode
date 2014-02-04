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
 * \file ed2kfwd.h Forward declarations of ED2K module classes
 */

#ifndef __ED2K_FWD_H__
#define __ED2K_FWD_H__

#include <boost/shared_ptr.hpp>

namespace Donkey {

class ED2K;
class ED2KConfig;
class ServerList;
class CreditsDb;
class Credits;
class Client;
class ClientList;
class Download;
class DownloadList;

struct ED2KNetProtocolTCP;
struct ED2KNetProtocolUDP;

template<typename, typename> class ED2KParser;

namespace ED2KPacket {
	class LoginRequest;
	class ServerMessage;
	class ServerStatus;
	class IdChange;
	class GetServerList;
	class ServerIdent;
	class ServerList;
	class OfferFiles;
	class Search;
	class SearchResult;
	class ReqCallback;
	class CallbackReq;
	class ReqSources;
	class FoundSources;
	class GlobGetSources;
	class GlobFoundSources;
	class GlobStatReq;
	class GlobStatRes;
	class Hello;
	class HelloAnswer;
	class MuleInfo;
	class MuleInfoAnswer;
	class ReqFile;
	class FileName;
	class FileDesc;
	class SetReqFileId;
	class NoFile;
	class FileStatus;
	class ReqHashSet;
	class HashSet;
	class StartUploadReq;
	class AcceptUploadReq;
	class QueueRanking;
	class MuleQueueRank;
	class ReqChunks;
	class DataChunk;
	class PackedChunk;
	class CancelTransfer;
	class SourceExchReq;
	class AnswerSources;
	class Message;
	class ChangeId;
	class SecIdentState;
	class PublicKey;
	class Signature;
	class ReaskFilePing;
	class QueueFull;
	class ReaskAck;
	class FileNotFound;
	class PortTest;
}
namespace Detail {
	class QueueInfo;
	class UploadInfo;
	class SourceInfo;
	class DownloadInfo;

	//! Client class extension types
	//!@{
	typedef boost::shared_ptr<QueueInfo   > QueueInfoPtr;
	typedef boost::shared_ptr<UploadInfo  > UploadInfoPtr;
	typedef boost::shared_ptr<SourceInfo  > SourceInfoPtr;
	typedef boost::shared_ptr<DownloadInfo> DownloadInfoPtr;
	//!@}
}

} // end namespace Donkey

#endif
