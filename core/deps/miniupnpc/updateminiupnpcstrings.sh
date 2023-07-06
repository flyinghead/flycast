#! /bin/sh
# $Id: updateminiupnpcstrings.sh,v 1.9 2021/09/28 21:37:53 nanard Exp $
# project miniupnp : http://miniupnp.free.fr/
# (c) 2009-2021 Thomas Bernard

FILE=miniupnpcstrings.h
TEMPLATE_FILE=${FILE}.in

if [ -n "$1" ] ; then
  FILE="$1"
fi
if [ -n "$2" ] ; then
  TEMPLATE_FILE="$2"
fi
TMPFILE=`mktemp -t miniupnpcstringsXXXXXX`
if [ ! -f "$TMPFILE" ] ; then
	echo "mktemp failure"
	exit 1
fi

# detecting the OS name and version
OS_NAME=`uname -s`
OS_VERSION=`uname -r`
if [ -f /etc/debian_version ]; then
	OS_NAME=Debian
	OS_VERSION=`cat /etc/debian_version`
fi

# use lsb_release (Linux Standard Base) when available
LSB_RELEASE=`which lsb_release`
if [ 0 -eq $? -a -x "${LSB_RELEASE}" ]; then
	# On NixOS, lsb_release returns strings such as "NixOS" (with quotes),
	# so we need to stript them with the following xargs trick:
	OS_NAME=`${LSB_RELEASE} -i -s | xargs echo`
	OS_VERSION=`${LSB_RELEASE} -r -s | xargs echo`
	case $OS_NAME in
		Debian)
			#OS_VERSION=`${LSB_RELEASE} -c -s`
			;;
		Ubuntu)
			#OS_VERSION=`${LSB_RELEASE} -c -s`
			;;
	esac
fi

# on AmigaOS 3, uname -r returns "unknown", so we use uname -v
if [ "$OS_NAME" = "AmigaOS" ]; then
	if [ "$OS_VERSION" = "unknown" ]; then
		OS_VERSION=`uname -v`
	fi
fi

echo "Detected OS [$OS_NAME] version [$OS_VERSION]"
MINIUPNPC_VERSION=`cat VERSION`
echo "MiniUPnPc version [${MINIUPNPC_VERSION}]"

EXPR="s|OS_STRING \".*\"|OS_STRING \"${OS_NAME}/${OS_VERSION}\"|"
#echo $EXPR
test -f ${FILE}.in
echo "setting OS_STRING macro value to ${OS_NAME}/${OS_VERSION} in $FILE."
sed -e "$EXPR" < $TEMPLATE_FILE > $TMPFILE

EXPR="s|MINIUPNPC_VERSION_STRING \".*\"|MINIUPNPC_VERSION_STRING \"${MINIUPNPC_VERSION}\"|"
echo "setting MINIUPNPC_VERSION_STRING macro value to ${MINIUPNPC_VERSION} in $FILE."
sed -e "$EXPR" < $TMPFILE > $FILE
rm $TMPFILE && echo "$TMPFILE deleted"

