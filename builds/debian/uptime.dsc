Format:         1.0
Source:         uptime
Version:        0.0.0-1
Binary:         libupt0, uptime-dev
Architecture:   any all
Maintainer:     John Doe <John.Doe@example.com>
Standards-Version: 3.9.5
Build-Depends: bison, debhelper (>= 8),
    pkg-config,
    automake,
    autoconf,
    libtool,
    libsodium-dev,
    libzmq4-dev,
    libczmq-dev,
    libmlm-dev,
    libbiosproto-dev,
    dh-autoreconf

Package-List:
 libupt0 deb net optional arch=any
 uptime-dev deb libdevel optional arch=any

