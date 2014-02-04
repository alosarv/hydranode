#
#             HOWTO build RPM packages using this hydranode-build.spec
#
# 1. Get a hydranode distributable tarball.
# 2. Copy the files
#   $ cp hydranode-%{version}.tar.gz /usr/src/packages/SOURCES
#   $ cp hydranode-build.spec /usr/src/packages/SPECS
# 3. Download a boost_%{version}.tar.bz2 and put it into /usr/src/packages/SOURCES
# 4. Build the RPMs
#   $ rpmbuild -ba /usr/src/packages/SPECS/hydranode-build.spec
#
#
# UNTESTED! Please contact agbr if you have any questions!
#
# Reference: http://forum.hydranode.com/showthread.php?tid=122
# You can send him an email/pn there.
#

%{!?boost_buildin:  %define      boost_buildin  1}
%{!?boost:          %define      boost          boost_1_33_0}

%define         prefix          /opt/hydranode
%define         _prefix         %{prefix}
%define         plugindir       %{_prefix}/plugins

Summary:        Modular MultiPlatform MultiNetwork P2P Client Framework
Name:           hydranode
Version:        0.1.2
Release:        2
Provides:       hydranode
Copyright:      GPL
Group:          Application/Internet
Source0:        hydranode-%{version}-src.tar.bz2
%if %boost_buildin
Source1:        http://heanet.dl.sourceforge.net/sourceforge/boost/%{boost}.tar.bz2
NoSource:       1
%endif
URL:            http://www.hydranode.org
Vendor:         Alo Sarv
# Packager:	YourNameHere       
BuildRoot:      %{_tmppath}/%{name}-%{version}-root

%description
Modular MultiPlatform MultiNetwork P2P Client Framework

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig


%package        ed2k
Group:          Application/Internet
Summary:        A ed2k plugin for hydranode
Requires:       hydranode

%description    ed2k
A ed2k plugin for Hydranode


%package        hnsh
Group:          Application/Internet
Summary:        A hydranode shell plugin for hydranode
Requires:       hydranode

%description    hnsh
A hydranode shell plugin for Hydranode, for telnet access to the core.


%package        devel
Group:          Development/Libraries/C and C++
Summary:        A hydranode include files
Requires:       hydranode

%description    devel
A hydranode include files


%package        mailnotify
Group:          Application/Internet
Summary:        A mailnotify plugin for hydranode
Requires:       hydranode

%description    mailnotify
A mailnotify plugin for hydranode


%package        http
Group:          Application/Internet
Summary:        A http plugin for hydranode
Requires:       hydranode

%description    http
Http - allows downloading of files with HTTP/1.1 protocol.


%package        cgcomm
Group:          Application/Internet
Summary:        A CGComm plugin for hydranode
Requires:       hydranode

%description    cgcomm
CGComm - provides TCP-based protocol for Engine / Interface communication.


%prep
%if %boost_buildin
%setup -b 1
%else
%setup
%endif

cat << EOPATCH | patch -p0
--- hncore/Jamfile 00:00:00.000000000 +0000
+++ hncore/Jamfile 00:00:00.000000000 +0000
@@ -25,5 +25,5 @@
 lib hncore
 : src/\$(CPP_SOURCES).cpp \$(deps)
-: <define>HNBASE_IMPORTS
+: <define>HNBASE_IMPORTS <define>'MODULE_DIR=\"%{plugindir}\"'
 ;
 
EOPATCH

%build        
%if %boost_buildin
%{!?boost_jam: pushd $RPM_BUILD_DIR/%{boost}/tools/build/jam_src && /bin/sh ./build.sh && popd}
%{!?boost_jam: %define boost_jam %(find %{_builddir}/%{boost} -name bjam)}
%{boost_jam} -d+2 '-sBOOST_ROOT=%{_builddir}/%{boost}' release
%else
%{!?boost_jam: %define boost_jam bjam}
%{boost_jam} -d+2 release
%endif

%install
# setup linker cache settings
%{__mkdir_p} $RPM_BUILD_ROOT/etc/ld.so.conf.d
echo %{_libdir} > $RPM_BUILD_ROOT/etc/ld.so.conf.d/hydranode.conf
# setup profile settings
%{__mkdir_p} $RPM_BUILD_ROOT/etc/profile.d
echo "export PATH=\$PATH:%{_bindir}" > $RPM_BUILD_ROOT/etc/profile.d/hydranode.sh
echo "setenv PATH=\${PATH}:%{_bindir}" > $RPM_BUILD_ROOT/etc/profile.d/hydranode.csh
# install
%{__mkdir_p} $RPM_BUILD_ROOT/%{_bindir}
%{__cp} -p hydranode $RPM_BUILD_ROOT/%{_bindir}/hydranode
%{__cp} -p hlink $RPM_BUILD_ROOT/%{_bindir}/hlink
%{__cp} -p -r lib $RPM_BUILD_ROOT/%{_libdir}
%{__cp} -p -r plugins $RPM_BUILD_ROOT/%{plugindir}
%{__cp} -p -r include $RPM_BUILD_ROOT/%{_includedir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog README
%dir %{prefix}
%dir %{_bindir}
%dir %{_libdir}
%dir %{_includedir}
%dir %{plugindir}
%{_bindir}/hydranode
%{_bindir}/hlink
%{_libdir}
/etc/ld.so.conf.d/hydranode.conf
/etc/profile.d/hydranode.sh
/etc/profile.d/hydranode.csh

%files ed2k
%defattr(-,root,root)
%{plugindir}/cmod_ed2k.so

%files hnsh
%defattr(-,root,root)
%{plugindir}/cmod_hnsh.so

%files mailnotify
%defattr(-,root,root)
%{plugindir}/cmod_mailnotify.so

%files http
%defattr(-,root,root)
%{plugindir}/cmod_http.so

%files cgcomm
%defattr(-,root,root)
%{plugindir}/cmod_cgcomm.so

%files devel
%defattr(-,root,root)
%{_includedir}/*
