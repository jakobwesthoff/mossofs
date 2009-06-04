#!/bin/bash

PROJECT="mossofs"

echo "Simple release packager (c) Jakob Westhoff"

if [ $# -lt 2 ]; then
	echo "  usage: $0 <release name> <target archive>"
	exit 1
fi

SVN="/usr/bin/svn"
TAR="/bin/tar"

# The releases path given relative to this script file in the repository
RELATIVE_RELEASES="../releases"

RELEASES="`readlink -f "$0"|xargs dirname`/${RELATIVE_RELEASES}"

RELEASE="$1"
TARGET="$2"

if [ ! -d "${RELEASES}/${RELEASE}" ]; then
	echo "Error: The specified release  \"${RELEASE}\" does not exist yet"
	exit 2
fi

PACKAGEDIR=`tempfile`
rm "${PACKAGEDIR}"
mkdir "${PACKAGEDIR}"
EXPORTDIR="${PACKAGEDIR}/${PROJECT}-${RELEASE}"

${SVN} export "${RELEASES}/${RELEASE}" "${EXPORTDIR}" \
&& ${TAR} cvzf "${TARGET}.tar.gz" -C "${PACKAGEDIR}" ./

rm -rf "${PACKAGEDIR}"

echo "Finished: The packaged file has been written to \"${TARGET}.tar.gz\""
