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

#ifndef __ED2K_FACTORIES_H__
#define __ED2K_FACTORIES_H__

namespace Donkey {

/**
 * \file factories.h
 * Uses macros to declare a set of factories for each and every packet we can
 * receive from eDonkey2000 network streams. This is an implementation header -
 * client code should never include this file directly - instead include
 * parser.h.
 */

/* Packets received from servers via TCP */
DECLARE_PACKET_FACTORY(PR_ED2K,  ServerStatus,      OP_SERVERSTATUS     );
DECLARE_PACKET_FACTORY(PR_ED2K,  IdChange,          OP_IDCHANGE         );
DECLARE_PACKET_FACTORY(PR_ED2K,  GetServerList,     OP_GETSERVERLIST    );
DECLARE_PACKET_FACTORY(PR_ED2K,  ServerList,        OP_SERVERLIST       );
DECLARE_PACKET_FACTORY(PR_ED2K,  ServerIdent,       OP_SERVERIDENT      );
DECLARE_PACKET_FACTORY(PR_ED2K,  ServerMessage,     OP_SERVERMESSAGE    );
DECLARE_PACKET_FACTORY(PR_ED2K,  SearchResult,      OP_SEARCHRESULT     );
DECLARE_PACKET_FACTORY(PR_ED2K,  CallbackReq,       OP_CBREQUESTED      );
DECLARE_PACKET_FACTORY(PR_ED2K,  FoundSources,      OP_FOUNDSOURCES     );

/* Packets received from other clients via TCP */
DECLARE_PACKET_FACTORY(PR_ED2K,  Hello,             OP_HELLO            );
DECLARE_PACKET_FACTORY(PR_ED2K,  HelloAnswer,       OP_HELLOANSWER      );
DECLARE_PACKET_FACTORY(PR_EMULE, MuleInfo,          OP_MULEINFO         );
DECLARE_PACKET_FACTORY(PR_EMULE, MuleInfoAnswer,    OP_MULEINFOANSWER   );
DECLARE_PACKET_FACTORY(PR_ED2K,  ReqFile,           OP_REQFILE          );
DECLARE_PACKET_FACTORY(PR_ED2K,  FileName,          OP_FILENAME         );
DECLARE_PACKET_FACTORY(PR_EMULE, FileDesc,          OP_FILEDESC         );
DECLARE_PACKET_FACTORY(PR_ED2K,  SetReqFileId,      OP_SETREQFILEID     );
DECLARE_PACKET_FACTORY(PR_ED2K,  NoFile,            OP_REQFILE_NOFILE   );
DECLARE_PACKET_FACTORY(PR_ED2K,  FileStatus,        OP_REQFILE_STATUS   );
DECLARE_PACKET_FACTORY(PR_ED2K,  ReqHashSet,        OP_REQHASHSET       );
DECLARE_PACKET_FACTORY(PR_ED2K,  HashSet,           OP_HASHSET          );
DECLARE_PACKET_FACTORY(PR_ED2K,  StartUploadReq,    OP_STARTUPLOADREQ   );
DECLARE_PACKET_FACTORY(PR_ED2K,  AcceptUploadReq,   OP_ACCEPTUPLOADREQ  );
DECLARE_PACKET_FACTORY(PR_ED2K,  QueueRanking,      OP_QUEUERANKING     );
DECLARE_PACKET_FACTORY(PR_EMULE, MuleQueueRank,     OP_MULEQUEUERANK    );
DECLARE_PACKET_FACTORY(PR_ED2K,  ReqChunks,         OP_REQCHUNKS        );
DECLARE_PACKET_FACTORY(PR_ED2K,  DataChunk,         OP_SENDINGCHUNK     );
DECLARE_PACKET_FACTORY(PR_EMULE, PackedChunk,       OP_PACKEDCHUNK      );
DECLARE_PACKET_FACTORY(PR_ED2K,  CancelTransfer,    OP_CANCELTRANSFER   );
DECLARE_PACKET_FACTORY(PR_EMULE, SourceExchReq,     OP_REQSOURCES       );
DECLARE_PACKET_FACTORY(PR_EMULE, AnswerSources,     OP_ANSWERSOURCES    );
DECLARE_PACKET_FACTORY(PR_ED2K,  AnswerSources2,    OP_ANSWERSOURCES    );
DECLARE_PACKET_FACTORY(PR_ED2K,  Message,           OP_MESSAGE          );
DECLARE_PACKET_FACTORY(PR_ED2K,  ChangeId,          OP_CHANGEID         );
DECLARE_PACKET_FACTORY(PR_EMULE, SecIdentState,     OP_SECIDENTSTATE    );
DECLARE_PACKET_FACTORY(PR_EMULE, PublicKey,         OP_PUBLICKEY        );
DECLARE_PACKET_FACTORY(PR_EMULE, Signature,         OP_SIGNATURE        );

/* Packets received via UDP socket are handled in-place and not here. */

}

#endif
