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

use warnings;
use RRDs;

#######

my $logfile   = $ENV{HOME}."/.hydranode/statistics.log";		# defaultvalue for the logfile
my $offsetfile= $ENV{HOME}."/.hydranode/.hnanalyze.offset";		# remembers our position in the logfile
my $datafile  = $ENV{HOME}."/.hydranode/hnanalyze.rrd";                 # where the RRD (compiled logdata) is saved
my $outputdir = $ENV{HOME}."/public_html";				# where the png's are saved
my $dst       = "MAX";                                  		# change to AVERAGE if you don't want the peaks in the graphs
									# you need to remove the RRD and the offsetfile for that change, 
									# it needs to be recreated.
my $entries   = 1;							# needs to be 1
my $size      = 0;
my $end       = 0;

#######

open(LOGFILE,'<',$logfile) || die "can't open $logfile: $!";
opendir(DIR, $outputdir) || die "can't open $outputdir for HTML output: $!"; closedir(DIR);

my ($inode, $ino, $offset) = (0, 0, 0);
unless (not $offsetfile) {
    if (open(OFFSET, $offsetfile)) {
        $_ = <OFFSET>;
        unless (! defined $_) {
            chomp $_;
            $inode = $_;
            $_ = <OFFSET>;
            unless (! defined $_) {
                chomp $_;
                $offset = $_;
            }
        }
    }

    unless ((undef,$ino,undef,undef,undef,undef,undef,$size) = stat $logfile) {
        print "Cannot get $logfile file size.\n", $logfile;
        exit 65;
    }

    if ($inode == $ino) {
        exit 0 if $offset == $size; # short cut
        if ($offset > $size) {
            $offset = 0;
            printf "WARNING - logfile is smaller than last time checked!\n";
	    printf "Restarting logfile from the beginning.\n"
        }
    }
    if ($inode != $ino || $offset > $size) {
        $offset = 0;
    }
    seek(LOGFILE, $offset, 0);
}

$_ = <LOGFILE>;
if (m/^(\d{10})\d\d\d \[ED2KStatistics\] (\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+)/) {
	($start,$srcs,$qlen,$up,$down,$conns,$upc,$downc,$uploadslots,$downloadslots)=($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
} elsif (m/^(\d{10})\d\d\d \[ED2KStatistics\] (\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+)/) {
	# Old, pre r2219 revision
	($start,$srcs,$qlen,$up,$down,$conns,$upc,$downc)=($1, $2, $3, $4, $5, $6, $7, $8);
	$uploadslots = "U"; $downloadslots = "U";
} else {
        printf "Looks like we have a problem the the logfile $logfile. The first line doesn't look like the right format.\n\n";
	printf "If you recently updated your revisions, please consider removing the following files:\n";
	printf "  $offsetfile\n";
	printf "  $datafile\n";
	printf "  $logfile\n";
	printf "\n";
        exit 10;
}

if (! -e "$datafile") {						# only create a new,empty rrd file if we're missing it
	seek(LOGFILE, 0, 0);					# seek back to the beginning of the log (if we moved in it before)
	printf "+ creating rra databases\n";
	RRDs::create(
    		$datafile, "--step=10",
    		"--start=".($start-30),
    		"DS:up:GAUGE:10:U:500000",
    		"DS:down:GAUGE:10:U:500000",
    		"DS:conns:GAUGE:10:U:50000",
    		"DS:srcs:GAUGE:10:U:500000",
   		"DS:qlen:GAUGE:10:U:500000",
    		"DS:upc:GAUGE:10:U:500000",
    		"DS:downc:GAUGE:10:U:500000",
		"DS:ups:GAUGE:10:U:1000",
		"DS:dos:GAUGE:10:U:1000",
    		"RRA:$dst:0.5:1:8600",
    		"RRA:$dst:0.5:6:8600",
    		"RRA:$dst:0.5:24:8600",
    		"RRA:$dst:0.5:288:8600"
		) or die "Cannot create rrd ($RRDs::error, $datafile)";
}

printf "+ inserting data into rrd\n";
if ($offset > 0) {
	printf "continuing: ".localtime($start)."\n";
} else {
	printf "     start: ".localtime($start)."\n";
}

# we need to catch the first line we used above for getting a start value, too
RRDs::update($datafile, "$start:$up:$down:$conns:$srcs:$qlen:$upc:$downc:$uploadslots:$downloadslots") or printf STDERR "Cannot update rrd ($!, $datafile),logline:\n$_\n";

while (<LOGFILE>) {
	if (m/^(\d{10})\d\d\d \[ED2KStatistics\] (\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+)/) {
		($timestamp,$srcs,$qlen,$up,$down,$conns,$upc,$downc,$uploadslots,$downloadslots)=($1, $2, $3, $4, $5, $6, $7, $8, $9, $10);
		$entries++;
		RRDs::update($datafile, "$timestamp:$up:$down:$conns:$srcs:$qlen:$upc:$downc:$uploadslots:$downloadslots") or printf STDERR "Cannot update rrd ($!, $datafile)\n";
	} elsif (m/^(\d{10})\d\d\d \[ED2KStatistics\] (\d+):(\d+):(\d+):(\d+):(\d+):(\d+):(\d+)/) {
		# old, pre r2219 logfile data
		($timestamp,$srcs,$qlen,$up,$down,$conns,$upc,$downc)=($1, $2, $3, $4, $5, $6, $7, $8);
		$entries++;
		RRDs::update($datafile, "$timestamp:$up:$down:$conns:$srcs:$qlen:$upc:$downc:U:U") or printf STDERR "Cannot update rrd ($!, $datafile)\n";
	} else {
		printf STDERR "Huh? Got a line which is unknown to me:\n$_\n";
	}

}

if ($entries > 1) {		# entries is 1 if we got only one line to read and skipped the above while loop.
	$end = $timestamp;
} else {
	$end = $start;		# no new log data, fixes warning messages
}

printf "       end: ".localtime($end)." ($entries entries)\n";
$size = tell LOGFILE;
close(LOGFILE);


# Disable output buffering at this point to enable the printing of belows dots without delay
$| = 1;				
printf "+ creating graphs ";

@timeperiods = (2*3600,12*3600,24*3600,3*24*3600,7*24*3600,30*24*3600,12*30*24*3600);

foreach $timespan (@timeperiods) {

   RRDs::graph($outputdir."/hn-traffic-$timespan.png",
    "--imgformat=PNG", "--vertical-label=traffic", "--start=".($end-$timespan),  "--end=".$end,
    "--lower-limit=0", "--width=900", "--height=200", "--step=10", "--base=1024","--lazy",
    "DEF:u=$datafile:up:$dst",
    "DEF:down=$datafile:down:$dst",
    "DEF:uc=$datafile:upc:$dst",
    "DEF:downc=$datafile:downc:$dst",
    "CDEF:up=u,-1,*",
    "CDEF:upc=uc,-1,*",
    "AREA:upc#1859B8:Upload Control    ",
    "GPRINT:uc:LAST:Current\\: %6.2lf %s",   
    "GPRINT:uc:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:uc:MAX:Maximum\\: %6.2lf %s\\n",
    "STACK:up#87B1EE:Upload Data       ",
    "GPRINT:u:LAST:Current\\: %6.2lf %s",
    "GPRINT:u:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:u:MAX:Maximum\\: %6.2lf %s\\n",
    "AREA:downc#948412:Download Control  ",   
    "GPRINT:downc:LAST:Current\\: %6.2lf %s",   
    "GPRINT:downc:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:downc:MAX:Maximum\\: %6.2lf %s\\n",
    "STACK:down#E1CA1B:Download Data     ",
    "GPRINT:down:LAST:Current\\: %6.2lf %s",
    "GPRINT:down:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:down:MAX:Maximum\\: %6.2lf %s\\n")
	or die "graph failed ($RRDs::error): traffic: $timespan\n";
	printf ".";

   RRDs::graph($outputdir."/hn-sources-$timespan.png",
    "--imgformat=PNG", "--vertical-label=sources", "--start=".($end-$timespan), "--end=".$end,
    "--lower-limit=0", "--width=900","--height=200","--lazy",
    "DEF:nsrc=$datafile:srcs:$dst",
    "DEF:qlen=$datafile:qlen:$dst",
    "DEF:conns=$datafile:conns:$dst",
    "AREA:qlen#7EE600:Queue Length ",
    "GPRINT:qlen:LAST: Current\\: %6.2lf %s",
    "GPRINT:qlen:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:qlen:MAX:Maximum\\: %6.2lf %s\\n",
    "LINE2:nsrc#FF5700:Sources      ",   
    "GPRINT:nsrc:LAST: Current\\: %6.2lf %s",
    "GPRINT:nsrc:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:nsrc:MAX:Maximum\\: %6.2lf %s\\n")
	or die "graph failed ($RRDs::error): sources: $timespan\n";
	printf ".";

   RRDs::graph($outputdir."/hn-slots-$timespan.png",
    "--imgformat=PNG", "--vertical-label=slots", "--start=".($end-$timespan),  "--end=".$end,
    "--lower-limit=0", "--width=900", "--height=200", "--step=10", "--lazy",
    "DEF:ups=$datafile:ups:$dst",
    "CDEF:upsc=ups,-1,*",
    "DEF:dos=$datafile:dos:$dst",
    "DEF:conns=$datafile:conns:$dst",
    "AREA:upsc#cc3300:Upload Slots    ",
    "GPRINT:ups:LAST:Current\\: %6.2lf %s",
    "GPRINT:ups:AVERAGE:Average\\: %6.2lf %s", 
    "GPRINT:ups:MAX:Maximum\\: %6.2lf %s\\n",
    "AREA:dos#3366ff:Download Slots  ",
    "GPRINT:dos:LAST:Current\\: %6.2lf %s",
    "GPRINT:dos:AVERAGE:Average\\: %6.2lf %s", 
    "GPRINT:dos:MAX:Maximum\\: %6.2lf %s\\n",
    "LINE1:conns#404040:Connections    ",
    "GPRINT:conns:LAST: Current\\: %6.2lf %s",
    "GPRINT:conns:AVERAGE:Average\\: %6.2lf %s",
    "GPRINT:conns:MAX:Maximum\\: %6.2lf %s\\n")
	or die "graph failed ($RRDs::error): slots: $timespan\n";
	printf ".";
}
printf "\n";

# saving session data

unless (open(OFFSET, ">$offsetfile")) {
        print "File $offsetfile cannot be created. Check your permissions.\n";
        exit 73;
    }
    print OFFSET "$ino\n$size\n";
    close OFFSET;
