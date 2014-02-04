#!/usr/bin/perl
# Copyright (C) 2004-2006 Alo Sarv <madcat_@users.sourceforge.net>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#
#
#

# =============================================================================
# For best results, you need to start hydranode with the commandline parameters 
# "-t ed2k.client,ed2k.clientlist,ed2k.deadsource,chunks,ed2k.secident"
#
# If you start hydranode within gdb, use:
# > run -t ed2k.client,ed2k.clientlist,ed2k.deadsource,chunks,ed2k.secident,ed2k.sourceexchange,ed2k.globsrc
#
#
#
#
#
#

use Time::Local;
$|++;

my $logfile = $ENV{HOME}."/.hydranode/hydranode.log";
open(LOGFILE,'<',$logfile) || die "can't open ".$logfile.": $!";
#open(LOGFILE, '<', "./config/hydranode.log"); # for win32

my %Client;
my %ClientUnknown;
my $aa = 0; #temporary
my $ab = 0; #temporary
my $starttime = "";
my $endtime = "";
my $runtime = "";
my $lastline = "";

my $fromserver      = 0; # Sources received from servers
my $fromudp         = 0; # Sources received via UDP
my $fromsrcex       = 0; # Sources received via SourceExchange
my $passive         = 0; # Sources received passively
my $statreq         = 0; # Number of servers pinged
my $statres         = 0; # Number of servers that answered
my $getsrc          = 0; # Number of UDP GetSources sent
my $gotsrc          = 0; # Number of UDP GotSources received
my $deadsrv         = 0; # Number of dead servers deleted
my $deadsrc         = 0; # Number of dead sources dropped
my $deadque         = 0; # Number of dead queued clients dropped
my $sec_id_ok       = 0; # Number of successful secure identifications
my $sec_id_fail     = 0; # Number of failed secure identifications
my $sec_id_ok_low   = 0; # Number of successful secure identifications (to LowID Clients)
my $sec_id_fail_low = 0; # Number of failed secure identifications (to LowID Clients)
my $low_calltot     = 0; # Number of LowID callback attempts
my $low_callok      = 0; # Number of successful LowID callbacks
my $low_callfail    = 0; # Number of failed LowID callbacks
my $cltotal         = 0; # Total number of clients interacted with
my $udpreasks       = 0; # Number of UDP reask attempts done
my $udptimeout      = 0; # Number of timed out UDP reasks
my $udpack          = 0; # Number of UDP ReaskAcks received
my $udpackPosZero   = 0; # Number of UDP ReaskAcks received with remote position 0 
my $udpnotfound     = 0; # Number of UDP NotFound received
my $udpquefull      = 0; # Number of UDP QueueFull received
my $reqchunks       = 0; # Number of requested chunks
my $complchunks     = 0; # Number of completed chunks downloadings
my $complpackedc    = 0; # Number of completed packed chunks
my $downloadsess    = 0; # Number of download sessions
my $downloaddata    = 0; # Amount of download data received in sessions
my $downloadlongest = 0; # Longest download session
my $uploadsess      = 0; # Number of upload sessions
my $uploaddata      = 0; # Amount of upload data sent in sessions
my $uploadlongest   = 0; # Longest upload session
my $faileddownnnp   = 0; # Failed download sessions because of a NNP source
my $faileddownsess  = 0; # Failed download sessions (0 bytes transfered)
my $failedupsess    = 0; # Failed upload sessions (0 bytes transfered) (Total, Low and High ID Clients)
my $failedupsessh   = 0; # Failed upload sessions on HighID Clients
my $corrupt         = 0; # Amount of corrupted data dropped
my $serverconnL	    = 0; # Connected to a server and got LowID (firewalled)
my $serverconnH	    = 0; # Connected to a server and got HighID (tcp port accessible)

my $dropclrecoverQ  = 0; # The number of clients that reconnect to us within the next hour since we dropped them
my $dropclrecoverL  = 0; # The number of clients that reconnect to us after an hour since we dropped them

my %client_nnp;
my %timestamp;

my $linecnt1 =  0; # Counts logfile lines up to 25'000 and prints dot
my $linecnt2 =  0; # Counts logfile lines up to 1'000'000 and resets dots

$_ = <LOGFILE>;
m/^\[(\d+)-(\w+)-(\d+) (\d+):(\d+):(\d+)\]/ && ( $starttime = timelocal($6,$5,$4,$3,$2,$1)  );

printf "Please wait, parsing logfile";
while (<LOGFILE>) {
	$linecnt1++;
	if ($linecnt1 % 25000 == 0) {
		if ($linecnt1 % 1000000 == 0) {
        	        printf "\r                                                                    \r";
                	printf "Please wait, parsing logfile";
	        } else {
			printf ".";
		}
	}
	if (m/ed2k.secident/) {
		if (m/Ident succeeded/) {
			if (m/\[\d+:\d+\] /) {
				# LowID Secure Ident
				$sec_id_ok_low++;
			} else {
				$sec_id_ok++;
			}
		} elsif (m/Ident failed!/) {
			if (m/\[\d+:\d+\] /) {
                                # LowID Secure Ident
                                $sec_id_fail_low++;
                        } else {
                                $sec_id_fail++;
                        }
		}
	} elsif (m/LowID/) {
		if (m/Performing/) {
			$low_calltot++;
		} elsif (m/succeeded/) {
			$low_callok++;
		} elsif (m/timed out/) {
			$low_callfail++;
		} elsif (m/Cannot do LowID<->LowID reask/) {
			$deadsrc++;
		}
	} elsif (m/\(Hello\)/) {
		if (m/ClientSoftware is Unknown (\S+)/) {
			$ClientUnknown{$1}++;
			$cltotal++;
		} elsif (m/ClientSoftware is (\w+)/) {
			$Client{$1}++;
			$cltotal++;
		}


		if (m/ClientSoftware is /) {
			# Ah, we spotted a new client. But perhaps it isn't really new? Lets check if we dropped him before.

			m/\[(\d+)-(\w+)-(\d+) (\d+):(\d+):(\d+)\] .+ \[(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\:\d+)\]/;    # get date and ip:port
                        # year=$1, month=$2, day=$3, hour=$4, minute=$5, second=$6, ipport=$7

			# if we "use warnings;" we need to check if $7 is defined first. I don't check for lowid clients here (no ip then)
			if (defined $timestamp{$7}) {		# see, we dropped it!
				if (timelocal($6,$5,$4,$3,$2,$1) - $timestamp{$7} <= 3600) {
					# we dropped it within the last hour!
						$dropclrecoverQ++;
					# forget about the dropped source, it recovered.
						delete $timestamp{$7};
				} elsif (timelocal($6,$5,$4,$3,$2,$1) - $timestamp{$7} > 3600) {
					$dropclrecoverL++;
					delete $timestamp{$7};
				}
			}
		}

	} elsif (m/chunk /) {
		if (m/Requesting chunk/) {
			$reqchunks++;
		} elsif (m/Completed chunk/) {
			$complchunks++;
		} elsif (m/Completed packed chunk/) {
			$complchunks++;
			$complpackedc++;
		} elsif (m/ \[(.+)\] Exception while sending chunk reqests: No more needed parts/) {
 	                # Remember that this client is NNP before we count a failed DownloadSession
                	$client_nnp{$1}++; 
		}
	} elsif (m/ New ID: (\d+)/) {
		# 0x00ffffff is the highest LowID possible
		if ($1 > 16777215) {
			$serverconnH++;
		} else {
			$serverconnL++;
		}
	} elsif (m/Passivly adding source/) {
		$passive++;
	} elsif (m/GlobGetSources/) {
		if (m/Received (\d+) sources/) {
			$fromudp += $1;
			$gotsrc++;
		} elsif (m/Sending GlobGetSources/) {
			$getsrc++;
		}
	} elsif (m/GlobStat/) {
		if (m/Sending GlobStatReq/) {
			$statreq++;
		} elsif (m/Received GlobStatRes/) {
			$statres++;
		} elsif (m/Removing dead server/) {
			$deadsrv++;
		}
	} elsif (m/Source, but never connected/) {
		$deadsrc++;
	} elsif (m/Dropping client \(unable to connect\)/) {
		$deadsrc++;
	} elsif (m/Client is on different server/) {
		$deadsrc++;
	} elsif (m/Error performing LowID callback/) {
		$deadsrc++;
	} elsif (m/Unable to connect to client/) {
		$deadsrc++;
	} elsif (m/Queue update/) {
		if (m/(\d+) dropped/) {
			$deadque += $1;
		}
	} elsif (m/UDP Reask/) {
		if (m/in progress/) {
			$udpreasks++;
		} elsif (m/timed out/) {
			$udptimeout++;
		} elsif (m/Received ReaskAck/) {
			$udpack++;
			m/We are queued on position 0 for file/ && $udpackPosZero++;
		} elsif (m/Received FileNotFound/) {
			$udpnotfound++;
		} elsif (m/Received QueueFull/) {
			$udpquefull++;
		} elsif (m/and TCP Reask also failed/) {
			$deadsrc++;
			$dropcltcpfail++;		# This is ONLY for this match.

			m/\[(\d+)-(\w+)-(\d+) (\d+):(\d+):(\d+)\] .+ \[(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\:\d+)\]/;	# get date and ip:port
			# year=$1, month=$2, day=$3, hour=$4, minute=$5, second=$6, ipport=$7

			# format for timelocal: $sec,$min,$hour,$mday,$mon,$year
			# Remember when we dropped that ip:port
			$timestamp{$7} = timelocal($6,$5,$4,$3,$2,$1);
		}
	} elsif (m/Received (\d+) new sources from server/) {
		$fromserver += $1;
	} elsif (m/ \[(.+)\] DownloadSessionEnd: Total received: 0 bytes/) {
		if (defined($client_nnp{$1})) {
			# OK, this client was marked as a NNP, so this is not a failed downloadsession
			delete $client_nnp{$1};
			$faileddownnnp++;
		} else {
			$faileddownsess++;
		}
	} elsif (m/DownloadSessionEnd: Total received: (\d+) bytes/) {
		$downloadsess++;
		$downloaddata+=$1;
		if ($1 > $downloadlongest) {
			$downloadlongest=$1;
		}
	} elsif (m/UploadSessionEnd: Total sent: 0 bytes/) {
		$failedupsess++;
		if (m/ \[\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d+\] /) {
			# If we match a complete IP in that line, we failed to upload to a high id client
			$failedupsessh++;
		}
	} elsif (m/UploadSessionEnd: Total sent: (\d+) bytes/) {
		$uploadsess++;
		$uploaddata+=$1;
		if ($1 > $uploadlongest) {
			$uploadlongest=$1;
		}
	} elsif (m/Corruption/) {
		if (m/Corruption found at (\d+)..(\d+)/) {
			$corrupt += $2-$1;
		}
	} elsif (m/SourceExchange: Received (\d+) sources for/) {
		$fromsrcex += $1;
	}
	$lastline = $_;
}
close(LOGFILE);
printf "\r                                                                    \r";

if ($lastline =~ m/^\[(\d+)-(\w+)-(\d+) (\d+):(\d+):(\d+)\]/) { $endtime = timelocal($6,$5,$4,$3,$2,$1); }
$runtime = ($endtime - $starttime) / 60;

printf "---> Overall runtime for statistics below: ".sec2time($endtime - $starttime)."\n";

my $total = $fromserver+$passive+$fromudp+$fromsrcex;
if ($total and ($fromserver or $fromudp or $passive or $fromsrcex)) {
	printf "-> Sources acquisition statistics:\n";
	if ($fromserver) {
		printf "-----> From local server: %5d (%5.2f%%, %6.2f per minute)\n", 
			$fromserver, 
			$fromserver*100/$total, 
			$fromserver/$runtime;
	}
	if ($fromudp) {
		printf "-----> From UDP server:   %5d (%5.2f%%, %6.2f per minute)\n", 
			$fromudp, 
			$fromudp*100/$total, 
			$fromudp/$runtime;
	}
	if ($passive) {
		printf "-----> Passively:         %5d (%5.2f%%, %6.2f per minute)\n", 
			$passive, 
			$passive*100/$total, 
			$passive/$runtime;
	}
	if ($fromsrcex) {
		printf "-----> SourceExchange:    %5d (%5.2f%%, %6.2f per minute)\n", 
			$fromsrcex, 
			$fromsrcex*100/$total, 
			$fromsrcex/$runtime;
 	}
	printf "-----> Total:             %5d (        %6.2f per minute)\n", 
			$total, 
			$total/$runtime;
}

if ($deadsrc or $deadque) {
	printf "-> Clients dropping statistics:\n";
	if ($deadsrc && $total) {
		printf "-----> Dropped %d dead sources  (%5.2f%%,  %5.2f per minute)\n", 
			$deadsrc, 
			$deadsrc*100/($total+$deadsrc), 
			$deadsrc/$runtime;
	}
	if ($deadque) {
		printf "-----> Dropped %d dead clients from queue.\n", 
			$deadque;
	}
	if ($dropcltcpfail) {
		printf "-------> Dropped %d clients because of failed TCP reasks (%4.2f per minute).\n", 
			$dropcltcpfail, 
			$dropcltcpfail/$runtime;
		printf "-------> %4d recovered within the next hour (%5.2f%% 'false positive' drops)\n", 
			$dropclrecoverQ, 
			$dropclrecoverQ*100/$dropcltcpfail;
		printf "-------> %4d recovered later\n", 
			$dropclrecoverL;
	}
}
if ($sec_id_ok or $sec_id_fail or $low_calltot) {
	printf "-> Feature statistics:\n";
	if ($sec_id_ok or $sec_id_fail) {
		printf "-----> HighId: Succeeded %5d out of %5d SecIdents (%5.2f%%, %5.2f per minute)\n", 
			$sec_id_ok, 
			$sec_id_ok+$sec_id_fail, 
			$sec_id_ok*100/($sec_id_fail+$sec_id_ok), 
			($sec_id_ok+$sec_id_fail)/$runtime;
	}
	if ($sec_id_ok_low or $sec_id_fail_low) {
                printf "-----> LowId : Succeeded %5d out of %5d SecIdents (%5.2f%%, %5.2f per minute)\n", 
			$sec_id_ok_low, 
			$sec_id_ok_low+$sec_id_fail_low, 
			$sec_id_ok_low*100/($sec_id_fail_low+$sec_id_ok_low), 
			($sec_id_ok_low+$sec_id_fail_low)/$runtime;
        }
	if ($low_calltot) {
		printf "-----> Succeeded %5d out of %5d LowID callbacks   (%5.2f%%, %5.2f per minute)\n", 
			$low_callok, 
			$low_calltot, 
			$low_callok*100/$low_calltot, 
			$low_calltot/$runtime;
	}
	if ($udpreasks) {
		printf "-----> Succeeded %5d out of %5d UDP reasks        (%5.2f%%, %5.2f per minute)\n", 
			$udpreasks-$udptimeout, 
			$udpreasks, 
			($udpreasks-$udptimeout)*100/$udpreasks, 
			$udpreasks/$runtime;
		if ($udpack) {
			printf "----------> ReaskAck:  %5d (%5.2f%%)", 
				$udpack, 
				$udpack*100/$udpreasks;
			if ($udpackPosZero) {
				printf ", %5d (%5.2f%%) indicating Position 0!",
					$udpackPosZero,
					$udpackPosZero*100/$udpack;
			}
			printf "\n";
		}
		if ($udpquefull) {
			printf "----------> QueueFull: %5d (%5.2f%%)\n", 
				$udpquefull, 
				$udpquefull*100/$udpreasks;
		}
		if ($udpnotfound) {
			printf "----------> NoFile:    %5d (%5.2f%%)\n", 
				$udpnotfound, 
				$udpnotfound*100/$udpreasks;
		}
		if ($udptimeout) {
			printf "----------> Timeout:   %5d (%5.2f%%)\n", 
				$udptimeout, 
				$udptimeout*100/$udpreasks;
		}
	}
}
if ($statreq or $getsrc or $deadsrv or $serverconnH or $serverconnL) {
	printf "-> Server communication statistics:\n";
	if ($serverconnH or $serverconnL) {
		printf "-----> Connected to server %d times, and received High ID %d times.\n", 
			$serverconnH+$serverconnL, 
			$serverconnH;
	}
	if ($statreq) {
		printf "-----> Sent %d pings, got %d answers (%.2f%% lost)\n", 
			$statreq, 
			$statres, 
			100-$statres*100/$statreq;
	}
	if ($getsrc) {
		printf "-----> Sent %d GetSources, got %d answers (%.2f%% effectiveness)\n", 
			$getsrc, 
			$gotsrc, 
			$gotsrc*100/$getsrc;
	}
	if ($deadsrv) {
		printf "-----> Dropped %d dead servers.\n", 
			$deadsrv;
	}
}

printf "-> ClientSoftware statistics:\n";
foreach (sort { $Client{$b}<=>$Client{$a} } keys %Client) {
	printf "-----> %15s: %6d (%5.2f%%)\n", 
		$_, 
		$Client{$_}, 
		$Client{$_}*100/$cltotal;
}
foreach (sort { $ClientUnknown{$b}<=>$ClientUnknown{$a} } keys %ClientUnknown) {
	printf "-----> Unknown %7s: %6d (%5.2f%%)\n", 
		$_, 
		$ClientUnknown{$_}, 
		$ClientUnknown{$_}*100/$cltotal;
}
printf "-> Transfer statistics:\n";
if ($downloadsess) {
	printf("-----> Downloaded %7.2f MB during %4d sessions (%5.2f MB avg per session)\n", 
		$downloaddata/1024/1024, $downloadsess, $downloaddata/$downloadsess/1024/1024);
}
if ($uploadsess) {
	printf("-----> Uploaded   %7.2f MB during %4d sessions (%5.2f MB avg per session)\n",
		$uploaddata/1024/1024, $uploadsess, $uploaddata/$uploadsess/1024/1024);
}
if ($downloadsess or $faileddownsess or $faileddownnnp) {
	printf("-----> Longest download session: %7.2f MB",	$downloadlongest/1024/1024);
	if ($faileddownsess or $faileddownnnp) {
		printf(", failed %d sessions (%5.2f%%) and %d were NNP clients (%5.2f%%)\n", 
			$faileddownsess, 
			$faileddownsess*100/($downloadsess+$faileddownsess+$faileddownnnp),
			$faileddownnnp,
			$faileddownnnp*100/($downloadsess+$faileddownsess+$faileddownnnp));
	} else {
		printf("\n");
	}
}
if ($uploadsess) {
	printf("-----> Longest upload session:   %7.2f MB", $uploadlongest/1024/1024);
	if ($failedupsess) {
		printf(", failed %d sessions (%5.2f%%)\n", 
			$failedupsess, $failedupsess*100/($uploadsess+$failedupsess));
	} else {
		printf("\n");
	}
}
if ($failedupsessh) {
	printf("-----> Failed %d upload sessions to High ID clients (%5.2f%%)\n", 
		$failedupsess, $failedupsess*100/($uploadsess+$failedupsess));
}
if ($complchunks) {
	printf "-----> Completed %d out of %d chunks (%5.2f%%, %4.2f per minute )\n", 
		$complchunks, 
		$reqchunks, 
		$complchunks*100/$reqchunks, 
		($complchunks*100/$reqchunks)/$runtime;
}
if ($complchunks and $complpackedc) {
	printf "-------> ... out of which %4d were zlib compressed (%5.2f%%, %4.2f per minute )\n", 
		$complpackedc, 
		$complpackedc*100/$complchunks, 
		$complpackedc/$runtime;
}
if ($corrupt) {
	printf "-----> Lost due to curruption:   %7.2f MB\n", 
		$corrupt/1024/1024;
}

sub sec2time {
 my $s = shift; 
  if ($s < 60) { 
    $s = int $s; 
    return "1 second" if ($s == 1); 
    return sprintf("%.1f seconds", $s); 
  } elsif ($s < 86400) { 
    my $hours = int $s / 3600; 
    $s %= 3600; 
    my $mins = int $s / 60; 
    $s %= 60; 
    return sprintf("%2.2uh %2.2um", $hours, $mins);
  } 
  my $days = int $s / 86400; 
  $s %= 86400; 
  my $hours = int $s / 3600; 
  $s %= 3600; 
  my $mins = int $s / 60; 
  $s %= 60; 
  my $S = ($days == 1) ? "" : "s"; 
  return sprintf("%u day$S %2.2uh %2.2um", $days, $hours, $mins); 
}
