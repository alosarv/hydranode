#######################################################################
#
# SPEC file for building hydranode release rpms from pre-compiled
# binaries.
#
#
#
# This is mostly SuSE and only for x86.
# Please refer to "man build" for further details to build the package.
#
# For a quick build, put *.wrapper and hydranode-0.1.2-linux-x86.tar.bz2
# in /usr/src/packages/SOURCES and "rpmbuild -bb hydranode.spec"
#
# Roland Arendes <roland@arendes.de>
#######################################################################

%define name            hydranode
%define version         0.1.2
%define prefix          /opt/hydranode

#######################################################################

Summary: hydranode is a modular, plugin-driven, multinetwork p2p client
Name: %{name}
Version: %{version}
Release: 1
Provides: %{name}
Copyright: GPL
URL: http://www.hydranode.com
Group: System Environment/Daemons
Source0: hydranode-%{version}-linux-x86.tar.bz2
Source1: hydranode.wrapper
Source2: hlink.wrapper
AutoReqProv: no
BuildArch: i386
BuildRoot: %{_tmppath}/%{name}-buildroot
Vendor: Alo Sarv <madcat_@users.sourceforge.net>
Packager: Roland Arendes <roland@arendes.de>
%description
HydraNode is a modular, plugin-driven peer-to-peer client framework which is 
designed with true multi-network downloads in mind. It can be used directly 
via the built-in shell functionality, or via external user interfaces.


%prep
# will set the name of the build directory to the listed name.
%setup -n %{name}-%{version}

# actual build commands, as we're using precompiled binaries from 
# madcat we don't need to do anything here.
%build

# files owned by package after install
%files
%defattr(755,root,root)
# complete /opt/hydranode and subdirs
%{prefix}
# the wrapper!
/usr/bin/hydranode
/usr/bin/hlink
%doc COPYING AUTHORS README ChangeLog

# all files which need to be installed to %{prefix}
%install
install -d -m 755 %{buildroot}%{prefix}
install -d -m 755 %{buildroot}%{prefix}/lib
install -m 744 lib/boost_date_time.so %{buildroot}%{prefix}/lib/boost_date_time.so
install -m 744 lib/boost_filesystem.so %{buildroot}%{prefix}/lib/boost_filesystem.so
install -m 744 lib/boost_program_options.so %{buildroot}%{prefix}/lib/boost_program_options.so
install -m 744 lib/boost_signals.so %{buildroot}%{prefix}/lib/boost_signals.so
install -m 744 lib/boost_thread.so %{buildroot}%{prefix}/lib/boost_thread.so
install -m 744 lib/boost_regex.so %{buildroot}%{prefix}/lib/boost_regex.so
install -m 744 lib/hnbase.so %{buildroot}%{prefix}/lib/hnbase.so
install -m 744 lib/hncore.so %{buildroot}%{prefix}/lib/hncore.so
install -m 744 lib/zlib.so %{buildroot}%{prefix}/lib/zlib.so
install -m 744 lib/libstdc++.so.5 %{buildroot}%{prefix}/lib/libstdc++.so.5
install -d -m 755 %{buildroot}%{prefix}/bin
install -m 755 hlink %{buildroot}%{prefix}/bin/hlink
install -m 755 hydranode %{buildroot}%{prefix}/bin/hydranode
install -d -m 755 %{buildroot}%{prefix}/plugins
install -m 744 plugins/cmod_cgcomm.so %{buildroot}%{prefix}/plugins/cmod_cgcomm.so
install -m 744 plugins/cmod_ed2k.so %{buildroot}%{prefix}/plugins/cmod_ed2k.so
install -m 744 plugins/cmod_hnsh.so %{buildroot}%{prefix}/plugins/cmod_hnsh.so
install -m 744 plugins/cmod_http.so %{buildroot}%{prefix}/plugins/cmod_http.so
install -m 744 plugins/cmod_mailnotify.so %{buildroot}%{prefix}/plugins/cmod_mailnotify.so
install -d -m 755 %{buildroot}/usr/bin
install -m 755 %{SOURCE1} %{buildroot}/usr/bin/hydranode
install -m 755 %{SOURCE2} %{buildroot}/usr/bin/hlink

%clean
# for content checking
rm -rf $RPM_BUILD_ROOT

%changelog
* Fri Aug 12 2005 Roland Arendes <roland@arendes.de>
- first version
