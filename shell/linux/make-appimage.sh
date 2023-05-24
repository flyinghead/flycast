#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Syntax: $0 <source root dir> <artifact dir> <output dir>"
    exit 1
fi

SHLIBS=(
	libcurl.so.4
	libz.so.1
	liblua5.3.so.0
	libminiupnpc.so.17
	libgomp.so.1
	libao.so.4
	libpulse.so.0
	libzip.so.5
	libnghttp2.so.14
	libidn2.so.0
	librtmp.so.1
	libssh.so.4
	libpsl.so.5
	libssl.so.1.1
	libcrypto.so.1.1
	libgssapi_krb5.so.2
	libldap_r-2.4.so.2
	liblber-2.4.so.2
	libbrotlidec.so.1
	pulseaudio/libpulsecommon-13.99.so
	libdbus-1.so.3
	libbz2.so.1.0
	libunistring.so.2
	libgnutls.so.30
	libhogweed.so.5
	libnettle.so.7
	libgmp.so.10
	libkrb5.so.3
	libk5crypto.so.3
	libcom_err.so.2
	libkrb5support.so.0
	libresolv.so.2
	libsasl2.so.2
	libgssapi.so.3
	libbrotlicommon.so.1
	libxcb.so.1
	libsystemd.so.0
	libwrap.so.0
	libsndfile.so.1
	libasyncns.so.0
	libapparmor.so.1
	libp11-kit.so.0
	libtasn1.so.6
	libkeyutils.so.1
	libheimntlm.so.0
	libkrb5.so.26
	libasn1.so.8
	libhcrypto.so.4
	libroken.so.18
	libXau.so.6
	libXdmcp.so.6
	liblzma.so.5
	liblz4.so.1
	libgcrypt.so.20
	libnsl.so.1
	libFLAC.so.8
	libogg.so.0
	libvorbis.so.0
	libvorbisenc.so.2
	libffi.so.7
	libwind.so.0
	libheimbase.so.1
	libhx509.so.5
	libsqlite3.so.0
	libcrypt.so.1
	libbsd.so.0
)

if [ ! -f appimagetool-x86_64.AppImage ]; then
	wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
	chmod +x appimagetool-x86_64.AppImage
fi

SRCDIR=$1
ARTDIR=$2
OUTDIR=$3

rm -rf "$OUTDIR"
mkdir -p "$OUTDIR/usr/bin" "$OUTDIR/usr/lib" "$OUTDIR/usr/lib/pulseaudio" "$OUTDIR/usr/optional"

echo "Building checkrt"
mkdir -p "$OUTDIR/tmp"
pushd "$OUTDIR/tmp"
git clone https://github.com/darealshinji/linuxdeploy-plugin-checkrt
cd linuxdeploy-plugin-checkrt
git checkout ec6237791c5aeb4cbc1fa00a092d1f7befa58988
cd src
make
cp -a checkrt ../../../usr/optional
cd ../../..
rm -rf tmp
popd
if [ ! -f "$OUTDIR/usr/optional/checkrt" ]; then
	echo "checkrt build failed"
	exit 1
fi
"$OUTDIR/usr/optional/checkrt" --copy-libraries

cp -a "$ARTDIR/flycast" "$OUTDIR/usr/bin"
patchelf --set-rpath '$ORIGIN/../lib' "$OUTDIR/usr/bin/flycast"

echo "Copying system shared libraries"
for lib in "${SHLIBS[@]}"; do
	blib=$(basename "$lib")
	if [ -f "/lib/x86_64-linux-gnu/$lib" ]; then
		cp "/lib/x86_64-linux-gnu/$lib" "$OUTDIR/usr/lib/$blib"
	elif [ -f "$CHROOT/usr/lib/x86_64-linux-gnu/$lib" ]; then
		cp "$CHROOT/usr/lib/x86_64-linux-gnu/$lib" "$OUTDIR/usr/lib/$blib"
	elif [ -f "$CHROOT/lib/$lib" ]; then
		cp "$CHROOT/lib/$lib" "$OUTDIR/usr/lib/$blib"
	elif [ -f "$CHROOT/usr/lib/$lib" ]; then
		cp "$CHROOT/usr/lib/$lib" "$OUTDIR/usr/lib/$blib"
	else
		echo "*** Failed to find '$blib'"
		exit 1
	fi

	strip "$OUTDIR/usr/lib/$blib"
done

for so in $(find "$OUTDIR/usr/lib" -maxdepth 1); do
	if [ -f "$so" ]; then
		echo "Patching RPATH in ${so}"
		patchelf --set-rpath '$ORIGIN' "$so"
	fi
done

cp -a "$SRCDIR/shell/linux/flycast.desktop" "$SRCDIR/shell/linux/flycast.png" "$OUTDIR"

echo "Creating AppRun..."
cat > "$OUTDIR/AppRun" << EOF
#!/bin/sh
APPDIR=\$(dirname "\$0")
grep -qs SteamOS /etc/os-release || \
  if [ -x "\$APPDIR/usr/optional/checkrt" ]; then
	extra_libs="\$(\$APPDIR/usr/optional/checkrt)"
  fi
if [ -n "\$extra_libs" ]; then
	export LD_LIBRARY_PATH="\${extra_libs}\${LD_LIBRARY_PATH}"
	if [ -e "\$APPDIR/usr/optional/exec.so" ]; then
		export LD_PRELOAD="\$APPDIR/usr/optional/exec.so:\${LD_PRELOAD}"
	fi
fi
exec "\$APPDIR/usr/bin/flycast" "\$@"
EOF
chmod +x "$OUTDIR/AppRun"

echo "Generate AppImage"
ARCH=x86_64 ./appimagetool-x86_64.AppImage -v "$OUTDIR" "flycast-x86_64.AppImage"

