#
#    uptime - Compute Data Center uptime
#
#    Copyright (C) 2014 - 2015 Eaton                                        
#                                                                           
#    This program is free software; you can redistribute it and/or modify   
#    it under the terms of the GNU General Public License as published by   
#    the Free Software Foundation; either version 2 of the License, or      
#    (at your option) any later version.                                    
#                                                                           
#    This program is distributed in the hope that it will be useful,        
#    but WITHOUT ANY WARRANTY; without even the implied warranty of         
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
#    GNU General Public License for more details.                           
#                                                                           
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            
#

Name:           uptime
Version:        0.0.0
Release:        1
Summary:        compute data center uptime
License:        MIT
URL:            http://example.com/
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkg-config
BuildRequires:  systemd-devel
BuildRequires:  libsodium-devel
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  libbiosproto-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
uptime compute data center uptime.

%package -n libupt0
Group:          System/Libraries
Summary:        compute data center uptime

%description -n libupt0
uptime compute data center uptime.
This package contains shared library.

%post -n libupt0 -p /sbin/ldconfig
%postun -n libupt0 -p /sbin/ldconfig

%files -n libupt0
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libupt.so.*

%package devel
Summary:        compute data center uptime
Group:          System/Libraries
Requires:       libupt0 = %{version}
Requires:       libsodium-devel
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       libbiosproto-devel

%description devel
uptime compute data center uptime.
This package contains development files.

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libupt.so
%{_libdir}/pkgconfig/libupt.pc

%prep
%setup -q

%build
sh autogen.sh
%{configure} --with-systemd
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%doc COPYING
%{_bindir}/kpi-uptime
%{_prefix}/lib/systemd/system/kpi-uptime*.service

#file for type systemd-tmpfiles
%dir /usr/lib/tmpfiles.d
/usr/lib/tmpfiles.d/kpi-uptime.conf

%changelog
