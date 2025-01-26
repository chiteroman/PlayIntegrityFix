#!/bin/sh
PATH=/data/adb/ap/bin:/data/adb/ksu/bin:/data/adb/magisk:/data/data/com.termux/files/usr/bin:$PATH
MODDIR=/data/adb/modules/playintegrityfix
version=$(grep "^version=" $MODDIR/module.prop | sed 's/version=//g' )

echo "[+] PlayIntegrityFix $version"
echo "[+] $(basename "$0")"
printf "\n\n"

download_fail() {
	dl_domain=$(echo "$1" | awk -F[/:] '{print $4}')
	ping -c 1 -W 5 "$dl_domain" > /dev/null 2>&1 || {
		echo "[!] Unable to connect to $dl_domain, please check your internet connection and try again"
		sleep_pause
		exit 1
		}
	conflict_module=$(ls /data/adb/modules | grep busybox)
	for i in $conflict_module; do 
		echo "[!] Please remove $conflict_module and try again." 
	done
	echo "[!] download failed!"
	echo "[x] bailing out!"
	sleep_pause
	exit 1
}

download() { busybox wget -T 10 --no-check-certificate -qO - "$1" > "$2" || download_fail "$1"; }
if command -v curl > /dev/null 2>&1; then
	download() { curl --connect-timeout 10 -s "$1" > "$2" || download_fail "$1"; }
fi

sleep_pause() {
	# APatch and KernelSU needs this
	if [ -z "$MMRL" ] && { [ "$KSU" = "true" ] || [ "$APATCH" = "true" ]; }; then
		sleep 5
	fi
}

set_random_beta() {
    if [ "$(echo "$MODEL_LIST" | wc -l)" -ne "$(echo "$PRODUCT_LIST" | wc -l)" ]; then
        echo "Error: MODEL_LIST and PRODUCT_LIST have different lengths."
        sleep_pause
        exit 1
    fi
    count=$(echo "$MODEL_LIST" | wc -l)
    rand_index=$(( $$ % count ))
    MODEL=$(echo "$MODEL_LIST" | sed -n "$((rand_index + 1))p")
    PRODUCT=$(echo "$PRODUCT_LIST" | sed -n "$((rand_index + 1))p")
    DEVICE=$(echo "$PRODUCT" | sed 's/_beta//')
}

# lets try to use tmpfs for processing
TEMPDIR="$MODDIR/temp" #fallback
[ -w /sbin ] && TEMPDIR="/sbin/playintegrityfix"
[ -w /debug_ramdisk ] && TEMPDIR="/debug_ramdisk/playintegrityfix"
mkdir -p "$TEMPDIR"
cd "$TEMPDIR"

download https://developer.android.com/topic/generic-system-image/releases PIXEL_GSI_HTML
grep -m1 -o 'li>.*(Beta)' PIXEL_GSI_HTML | cut -d\> -f2
grep -m1 -o 'Date:.*' PIXEL_GSI_HTML

current_timestamp=$(date +%s)
threshold_timestamp=1740312000 # Baklava still not name as 16. TODO: Need to know what is the actually time of changeing the name of release.
if [ "$current_timestamp" -lt "$threshold_timestamp" ]; then
    RELEASE="Baklava"
else
    RELEASE="$(grep -m1 'corresponding Google Pixel builds' PIXEL_GSI_HTML | grep -o '/versions/.*' | cut -d/ -f3)"
fi
ID="$(grep -m1 -o 'Build:.*' PIXEL_GSI_HTML | cut -d' ' -f2)"
INCREMENTAL="$(grep -m1 -o "$ID-.*-" PIXEL_GSI_HTML | cut -d- -f2)"

download "https://developer.android.com$(grep -m1 'corresponding Google Pixel builds' PIXEL_GSI_HTML | grep -o 'href.*' | cut -d\" -f2)" PIXEL_GET_HTML
download "https://developer.android.com$(grep -m1 'Pixel downloads page' PIXEL_GET_HTML | grep -o 'href.*' | cut -d\" -f2)" PIXEL_BETA_HTML

MODEL_LIST="$(grep -A1 'tr id=' PIXEL_BETA_HTML | grep 'td' | sed 's;.*<td>\(.*\)</td>;\1;')"
PRODUCT_LIST="$(grep -o 'factory/.*_beta' PIXEL_BETA_HTML | cut -d/ -f2)"

download https://source.android.com/docs/security/bulletin/pixel PIXEL_SECBULL_HTML

SECURITY_PATCH="$(grep -A15 "$(grep -m1 -o 'Security patch level:.*' PIXEL_GSI_HTML | cut -d' ' -f4-)" PIXEL_SECBULL_HTML | grep -m1 -B1 '</tr>' | grep 'td' | sed 's;.*<td>\(.*\)</td>;\1;')"

echo "- Selecting Pixel Beta device ..."
[ -z "$PRODUCT" ] && set_random_beta
echo "$MODEL ($PRODUCT)"

sdk_version="$(getprop ro.build.version.sdk)"
sdk_version="${sdk_version:-25}"
echo "Device SDK version: $sdk_version"

echo "- Dumping values to pif.json ..."
cat <<EOF | tee pif.json
{
  "FINGERPRINT": "google/$PRODUCT/$DEVICE:$RELEASE/$ID/$INCREMENTAL:user/release-keys",
  "MANUFACTURER": "Google",
  "MODEL": "$MODEL",
  "SECURITY_PATCH": "$SECURITY_PATCH",
  "DEVICE_INITIAL_SDK_INT": $sdk_version
}
EOF

cat "$TEMPDIR/pif.json" > /data/adb/pif.json
echo "- new pif.json saved to /data/adb/pif.json"

echo "- Cleaning up ..."
rm -rf "$TEMPDIR"

for i in $(busybox pidof com.google.android.gms.unstable); do
	echo "- Killing pid $i"
	kill -9 "$i"
done

echo "- Done!"
sleep_pause
