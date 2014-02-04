#!/usr/bin/perl
#/*
# *  Copyright (C) 2005-2006 Roland Arendes <roland@arendes.de>
# *
# *  This program is free software; you can redistribute it and/or modify
# *  it under the terms of the GNU General Public License as published by
# *  the Free Software Foundation; either version 2 of the License, or
# *  (at your option) any later version.
# *
# *  This program is distributed in the hope that it will be useful,
# *  but WITHOUT ANY WARRANTY; without even the implied warranty of
# *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# *  GNU General Public License for more details.
# *
# *  You should have received a copy of the GNU General Public License
# *  along with this program; if not, write to the Free Software
# *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# *
# *  Credits go to Jonathan Middleton <jjm@ixtab.org.uk> and
# *  Paul Slootman <paul@debian.org>, for writing logtail (part of Logcheck)
# */

open(LOGFILE, '<', $ENV{HOME}."/.hydranode/hydranode.log") || die "Could not open file: $!\n";
	while (<LOGFILE>) {
		# sample line
		# [2005-Oct-30 17:42:27] Trace(ed2k.client): [84.9.148.207:4662] Sending QueueRanking 925.
		# [2005-Oct-30 17:56:53] Trace(ed2k.client): [14913758:4662] 0x88026a8: Performing LowID callback...

		# from the tracemask "chunks"
		# [2006-Jan-27 01:36:57] Trace(chunks): [15557483:6346] eDonkey2000 chunkmap: 01
		# [2006-Jan-27 01:36:57] Trace(chunks): Failure to select chunk, did 1 hops

		if (m/^\[(.+)\] .+\[(.+:\d+)\] (.+)$/) {
			$datetime = $1; $ipport = $2; $logline = $3;
			$clientlog{"$ipport"} .= $datetime.": ".$logline."\n";
			
			if ($logline =~ m/eDonkey2000 chunkmap/) {
				# next line is supposed to be the chunk selection, add it to the list of the last known ip/port
				$_ = <LOGFILE>;
				m/.+ Trace\(chunks\): (.+)$/;		
				$clientlog{"$ipport"} .= $datetime.": eDonkey2000 chunkmap: ".$1."\n";
			}
		}
	}
close(LOGFILE);

for $client (keys %clientlog) {
		print $client."\n".$clientlog{$client}."\n";
}
