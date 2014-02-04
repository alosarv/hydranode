#!/usr/bin/perl
#
# Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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

use strict;
use warnings;
use Getopt::Std;

my %opts;
my @ignoredfiles;

getopt('i', \%opts);

if($opts{i}) {
	@ignoredfiles = split /,/, $opts{i};
}

my $searchdir = ($_ = shift @ARGV) ? $_ : '.';
my $re = ($_ = shift @ARGV) ? $_ : '(\.cpp$)|(\.h$)';
my $tablength = 8;

my $tab_sub = ' 'x$tablength;
my $tab_see = '<'.'!'x($tablength - 2).'>';

sub usage {
	my ($me) = @_;
	print "$me -i str[,str2,..] [directory] [pattern]\n";
	print "\t-i ignore files with name containing str[,str2,..]\n";
	print "\tif no directory specified, '.' is assumed\n";
	print "\tif no pattern is specified, is user $re\n";

	exit 1;
}

sub checkfile {
	my ($file) = @_;

	open(FILE, $file) || die "can't open $file: $!";

	my $line = 1;

	while(<FILE>) {
			# removes endline (both unix or windows)
			$_ =~ s/\r?\n$//;

			my $etab = $_;
			$etab =~ s/\t/$tab_sub/g;

			if($_ =~ /[^\t]\t/) {
				print "$file:$line: tab not at the beginning:\n";
				$_ =~ s/\t/$tab_see/g;
				print "$_\n\n";
			} elsif(($_ = (length $etab)) > 80) {
				print "$file:$line: more than 80 characters ($_):\n";

				$etab =~ s/^(.{80})(.*)/$1<<<$2>>>/;

				print "$etab\n\n";
			}

			$line++;
	}

	close FILE;
}

sub rsearch {
	my ($path) = @_;

	$path =~ s@//@/@g;

	# Check a single file
	if(-f $path) {
		checkfile($path);

		return;
	}

	# Check a directory
	opendir(DIR, $path) || die "can't opendir $path: $!";

	my @files = grep { !/^\./ } readdir(DIR);

	foreach my $i(@files) {
		if(-d $path.'/'.$i) {
			rsearch($path.'/'.$i);
		} elsif($i =~ /$re/) {
			my $f = 1;

			foreach my $j(@ignoredfiles) {
				if(($path.'/'.$i) =~ /$j/) {
					print "Ignored $path/$i (matches $j)\n";

					$f = 0;
				}
			}

			checkfile($path."/".$i) if($f == 1);
		}
	}

	closedir DIR;
}

rsearch $searchdir;
