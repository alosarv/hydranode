
Hydranode
======================================================
Modular MultiPlatform P2P Client Framework
------------------------------------------

====

Historical Notes
----------------

I developed this project between 2004 until 2006 as means of learning advanced C++ programming. 
The original website is still up at http://www.hydranode.com/, where you can also download 
precompiled binaries.

There are no guarantees at this point that any of the code compiles with modern compilers, nor that the
client actually works well (or at all) in the eDonkey2000 and BitTorrent networks.

I'm making this codebase available on GitHub in the hopes that someone might be able to find useful bits
in here, may it be for educational or other reasons.

What follows is the original README from 2006.

====



Compilation
-------------

  Hydranode consists of many components, plugins and libraries, many of
  which have external dependancies. In order to compile Hydranode, you need the
  following tools/libraries installed on system:

 * bjam executable for your platform
	Windows:   http://prdownloads.sourceforge.net/boost/boost-jam-3.1.11-1-ntx86.zip?download
	Linux:     http://prdownloads.sourceforge.net/boost/boost-jam-3.1.11-1-linuxx86.tgz?download
	Mac OS X:  http://prdownloads.sourceforge.net/boost/boost-jam-3.1.11-1-macosxppc.tgz?download
	Linux/RPM: http://prdownloads.sourceforge.net/boost/boost-jam-3.1.11-1.i386.rpm?download
	Source:    http://prdownloads.sourceforge.net/boost/boost-jam-3.1.11.tgz?download

 * C++ compiler
 	Windows:   http://prdownloads.sourceforge.net/mingw/MinGW-4.1.0.exe?download
 	Linux/Mac: Consult your distribution vendor; most likely you already have it installed.

 * Boost C++ libraries
	All platforms: http://tinyurl.com/cnhhu

 	Unpack them to C:\ (on windows) or to your HOME directory (unix).

 	NOTE: You do not need to compile them, Hydranode only uses headers from
 	      the Boost libraries source.

 * Trolltech QT4 GUI library (only required for user interfaces)
 	Windows:   http://www.trolltech.com/download/qt/windows.html
 	Unix/X11:  http://www.trolltech.com/download/qt/x11.html
 	Mac OS X:  http://www.trolltech.com/download/qt/mac.html

  * Update environment variables
        BOOST_ROOT Path to boost installation directory; defaults to c:\boost_1_33_1 on Windows,
	           $(HOME)/boost_1_33_1 on POSIX.
	QTDIR     Path to Qt 4 installation directory; required to be set on Windows; defaults
	          to /usr/local/Trolltech/Qt-4.1.1 on POSIX systems.

  Once you have all that, run "bjam release" in the Hydranode source tree.
  Hydranode Engine, it's support libraries and plugins will be compiled. The
  main executable is "hydranode". "hlink" executable allows sending links to
  a running Hydranode. Supplemental executables include 'pfwd' (for UPnP port forwarding),
  'bget' (allows directly downloading torrents) and 'httpget' (same as bget, for http
  links).

  To compile Hydranode Graphical User Interface, run "bjam release hngui". The
  executable is created at hngui/release subdir. Note that the interface loads
  it's skin images from backgrounds/ subdir from the executable, so you need to
  copy the hngui/backgrounds dir to hngui/release/backgrounds. The interface also
  requires Qt4 libraries to be copied to it's directory (or added to PATH 
  environment variable); the required DLL's are QtCore4, QtGui4, QtNetwork4 and
  QtXml4.

  The user interface attempts to connect to Hydranode Engine on port 9990 on 
  localhost by default (cgcomm module must be loaded in core for this to work).
  Failing that, the interface starts core process internally, looking for core
  executable named 'hydranode-core.exe' in current directory. Note that the core
  executable name is different from what is created when you compile the core, so
  renaming is required. This design decision was done to clarify which process is
  core and which is interface.

Usage
-----

  There are two ways you can use Hydranode. One way (easiest, recommended) is to
  simply start hydranode-gui executable, and let it run core process internally.
  This is also the default behaviour.

  Alternatively, you can run core and gui processes separately. To do that, run
  the core executable (hydranode or hydranode-core) separately. Make sure you
  also load the cgcomm module (enabled by default since 0.3 release). For
  list of command-line options, pass --help parameter to the core executable.

  With core running, you can either use the shell interface (available since 0.1
  version) or cgcomm / graphical interface (available since 0.3 version). If you
  are running the core on a different box than the interface, you need to pass
  the ip/port of the core machine to the interface, e.g. 'hydranode-gui.exe
  192.168.0.1 9990'.

  Hydranode stores all it's configuration files in config/ subdir (on windows),
  or $(HOME)/.hydranode directory (on POSIX systems). Changing config.ini or
  gui.ini while core or interface is running is not supported - use hnshell's
  'config' command or user interface Settings page to make changes instead.

  Completed downloads are placed to config/incoming directory, but it's possible
  to set incoming directory for each download separately (from user interface).

  Both cgcomm and hnshell accept connections from localhost only by default. If
  you want to allow remote connections, set the 'ListenIp' setting in the
  corresponding section of config.ini to the IP to listen on, e.g. if you want to
  allow connections only from your LAN, set it to your LAN IP (192.168.0.1 for
  example). If you want to allow connections from anywhere (not recommended),
  set it to 0.

More help or support
--------------------

  Should you run into trouble using Hydranode, have feature requests or found
  a bug, feel free to contact us either via the Support Forums at
  http://forum.hydranode.com, or posting a Support Ticket at
  http://dev.hydranode.com/newticket.

  Should you need to contact the developers directly, send an e-mail to
  madcat (at) hydranode (dot) com.

-----------------------------------------

 Happy downloading,

 Alo Sarv - madcat (at) hydranode (dot) com,
 on behalf of Hydranode Development Team.

-----------------------------------------
