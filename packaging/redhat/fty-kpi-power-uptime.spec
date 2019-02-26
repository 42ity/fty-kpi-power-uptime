#
#    fty-kpi-power-uptime - Compute Data Center uptime
#
#    Copyright (C) 2014 - 2018 Eaton
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

# To build with draft APIs, use "--with drafts" in rpmbuild for local builds or add
#   Macros:
#   %_with_drafts 1
# at the BOTTOM of the OBS prjconf
%bcond_with drafts
%if %{with drafts}
%define DRAFTS yes
%else
%define DRAFTS no
%endif
%define SYSTEMD_UNIT_DIR %(pkg-config --variable=systemdsystemunitdir systemd)
Name:           fty-kpi-power-uptime
Version:        1.0.0
Release:        1
Summary:        compute data center uptime
License:        GPL-2.0+
URL:            https://42ity.org
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
# Note: ghostscript is required by graphviz which is required by
#       asciidoc. On Fedora 24 the ghostscript dependencies cannot
#       be resolved automatically. Thus add working dependency here!
BuildRequires:  ghostscript
BuildRequires:  asciidoc
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkgconfig
BuildRequires:  systemd-devel
BuildRequires:  systemd
%{?systemd_requires}
BuildRequires:  xmlto
BuildRequires:  gcc-c++
BuildRequires:  libsodium-devel
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  log4cplus-devel
BuildRequires:  fty-common-logging-devel
BuildRequires:  fty-proto-devel
BuildRequires:  fty_shm-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
fty-kpi-power-uptime compute data center uptime.

%package -n libfty_kpi_power_uptime1
Group:          System/Libraries
Summary:        compute data center uptime shared library

%description -n libfty_kpi_power_uptime1
This package contains shared library for fty-kpi-power-uptime: compute data center uptime

%post -n libfty_kpi_power_uptime1 -p /sbin/ldconfig
%postun -n libfty_kpi_power_uptime1 -p /sbin/ldconfig

%files -n libfty_kpi_power_uptime1
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libfty_kpi_power_uptime.so.*

%package devel
Summary:        compute data center uptime
Group:          System/Libraries
Requires:       libfty_kpi_power_uptime1 = %{version}
Requires:       libsodium-devel
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       log4cplus-devel
Requires:       fty-common-logging-devel
Requires:       fty-proto-devel
Requires:       fty_shm-devel

%description devel
compute data center uptime development tools
This package contains development files for fty-kpi-power-uptime: compute data center uptime

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libfty_kpi_power_uptime.so
%{_libdir}/pkgconfig/libfty_kpi_power_uptime.pc
%{_mandir}/man3/*
%{_mandir}/man7/*

%prep

%setup -q

%build
sh autogen.sh
%{configure} --enable-drafts=%{DRAFTS} --with-systemd-units
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%doc README.md
%doc COPYING
%{_bindir}/fty-kpi-power-uptime
%{_mandir}/man1/fty-kpi-power-uptime*
%{_bindir}/fty-kpi-power-uptime-convert
%{_mandir}/man1/fty-kpi-power-uptime-convert*
%config(noreplace) %{_sysconfdir}/fty-kpi-power-uptime/fty-kpi-power-uptime.cfg
%{SYSTEMD_UNIT_DIR}/fty-kpi-power-uptime.service
%dir %{_sysconfdir}/fty-kpi-power-uptime
%if 0%{?suse_version} > 1315
%post
%systemd_post fty-kpi-power-uptime.service
%preun
%systemd_preun fty-kpi-power-uptime.service
%postun
%systemd_postun_with_restart fty-kpi-power-uptime.service
%endif

%changelog
