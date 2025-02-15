#!/bin/bash

set -x -e -o pipefail

export DEBEMAIL='packaging@wand.net.nz'
export DEBFULLNAME='WAND Packaging'
export DEBIAN_FRONTEND=noninteractive

export SOURCENAME=`echo ${GITHUB_REF##*/} | cut -d '-' -f 1`

apt-get update
apt-get install -y equivs devscripts dpkg-dev quilt curl apt-transport-https \
    apt-utils ssl-cert ca-certificates gnupg lsb-release debhelper git \
    pkg-config sed

DISTRO=$(lsb_release -sc)

curl -1sLf 'https://dl.cloudsmith.io/public/wand/libwandio/cfg/setup/bash.deb.sh' | bash
curl -1sLf 'https://dl.cloudsmith.io/public/wand/libwandder/cfg/setup/bash.deb.sh' | bash
curl -1sLf 'https://dl.cloudsmith.io/public/wand/libtrace/cfg/setup/bash.deb.sh' | bash
curl -1sLf 'https://dl.cloudsmith.io/public/wand/openli/cfg/setup/bash.deb.sh' | bash

case ${DISTRO} in
        xenial )
                curl -1sLf 'https://dl.cloudsmith.io/public/wand/dpdk-wand/cfg/setup/bash.deb.sh' | bash
                apt-get install -y debhelper dh-systemd -t xenial-backports
                sed -i 's/debhelper-compat (= 12)/debhelper (>= 10)/' debian/control
                sed -i 's/--with auto/--with=systemd --with auto/' debian/rules
                echo "10" > debian/compat
        ;;

        stretch )
                curl -1sLf 'https://dl.cloudsmith.io/public/wand/dpdk-wand/cfg/setup/bash.deb.sh' | bash
                sed -i 's/debhelper-compat (= 12)/debhelper (>= 10)/' debian/control
                sed -i 's/--with auto/--with=systemd --with auto/' debian/rules
                echo "10" > debian/compat
        ;;

        bullseye )
                sed -i 's/ dh-systemd (>=1.5),//' debian/control
        ;;

        bionic )
                apt-get install -y debhelper -t bionic-backports
        ;;
esac


apt-get update
apt-get upgrade -y
