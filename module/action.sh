#!/bin/sh

PATH=/data/adb/ap/bin:/data/adb/ksu/bin:/data/adb/magisk:/data/data/com.termux/files/usr/bin:$PATH
MODDIR=/data/adb/modules/playintegrityfix
version=$(grep "^version=" $MODDIR/module.prop | sed 's/version=//g')
FORCE_PREVIEW=1

TEMPDIR="$MODDIR/temp"
[ -w /sbin ] && TEMPDIR="/sbin/playintegrityfix"
[ -w /debug_ramdisk ] && TEMPDIR="/debug_ramdisk/playintegrityfix"
[ -w /dev ] && TEMPDIR="/dev/playintegrityfix"
mkdir -p "$TEMPDIR"
cd "$TEMPDIR" || exit 1

echo "[+] PlayIntegrityFix $version"
echo "[+] $(basename "$0")"
printf "\n\n"

sleep_pause() {
	if [ -z "$MMRL" ] && [ -z "$KSU_NEXT" ] && { [ "$KSU" = "true" ] || [ "$APATCH" = "true" ]; }; then
		sleep 5
	fi
}

download_fail() {
	local url_failed="$1"
	local dl_domain
	dl_domain=$(echo "$url_failed" | awk -F[/:] '{print $4}')

	rm -rf "$TEMPDIR"
	ping -c 1 -W 5 "$dl_domain" > /dev/null 2>&1 || {
		echo "[!] Unable to connect to $dl_domain, please check your internet connection and try again"
		sleep_pause
		exit 1
	}
	local conflict_module
	conflict_module=$(ls /data/adb/modules 2>/dev/null | grep -i 'busybox')
	for i in $conflict_module; do
		echo "[!] Potential conflict: module $i found. If issues persist, consider removing it."
	done
	echo "[!] Download failed for $url_failed"
	echo "[x] Bailing out!"
	sleep_pause
	exit 1
}

_download_wget() {
  local url="$1"
  local output_file="$2"
  
  busybox wget -T 10 --no-check-certificate -qO - "$url" > "$output_file" || {
    rm -f "$output_file"
    download_fail "$url"
  }
}

download() {
  _download_wget "$@"
}

if command -v curl > /dev/null 2>&1; then
  _download_curl() {
    local url="$1"
    local output_file="$2"
    local max_bytes="$3"
    local ret_code

    if [ -n "$max_bytes" ]; then
      curl --connect-timeout 10 -s -L --max-filesize "$max_bytes" "$url" -o "$output_file"
      ret_code=$?
      if [ $ret_code -ne 0 ] && [ $ret_code -ne 63 ]; then
        rm -f "$output_file"
        download_fail "$url"
      fi
    else
      curl --connect-timeout 10 -s -L "$url" -o "$output_file"
      ret_code=$?
      if [ $ret_code -ne 0 ]; then
        rm -f "$output_file"
        download_fail "$url"
      fi
    fi
  }
  download() {
    _download_curl "$@"
  }
fi

set_random_beta() {
	if [ "$(echo "$MODEL_LIST" | wc -l)" -ne "$(echo "$PRODUCT_LIST" | wc -l)" ]; then
		echo "Error: MODEL_LIST and PRODUCT_LIST have different lengths."
		sleep_pause
		exit 1
	fi
	local count
	count=$(echo "$MODEL_LIST" | wc -l)
	local rand_index
	rand_index=$(( $$ % count ))
	MODEL=$(echo "$MODEL_LIST" | sed -n "$((rand_index + 1))p")
	PRODUCT=$(echo "$PRODUCT_LIST" | sed -n "$((rand_index + 1))p")
}

download https://developer.android.com/about/versions PIXEL_VERSIONS_HTML
BETA_URL=$(grep -o 'https://developer.android.com/about/versions/.*[0-9]"' PIXEL_VERSIONS_HTML | sort -ru | cut -d\" -f1 | head -n1)
download "$BETA_URL" PIXEL_LATEST_HTML

if grep -qE 'Developer Preview|tooltip>.*preview program' PIXEL_LATEST_HTML && [ "$FORCE_PREVIEW" = 0 ]; then
	BETA_URL=$(grep -o 'https://developer.android.com/about/versions/.*[0-9]"' PIXEL_VERSIONS_HTML | sort -ru | cut -d\" -f1 | head -n2 | tail -n1)
	download "$BETA_URL" PIXEL_BETA_HTML
else
	mv -f PIXEL_LATEST_HTML PIXEL_BETA_HTML
fi

OTA_URL="https://developer.android.com$(grep -o 'href=".*download-ota.*"' PIXEL_BETA_HTML | cut -d\" -f2 | head -n1)"
download "$OTA_URL" PIXEL_OTA_HTML

MODEL_LIST="$(grep -A1 'tr id=' PIXEL_OTA_HTML | grep 'td' | sed 's;.*<td>\(.*\)</td>;\1;')"
PRODUCT_LIST="$(grep -o 'ota/.*_beta' PIXEL_OTA_HTML | cut -d\/ -f2)"
OTA_LIST="$(grep 'ota/.*_beta' PIXEL_OTA_HTML | cut -d\" -f2)"

echo "- Selecting Pixel Beta device ..."
[ -z "$PRODUCT" ] && set_random_beta
echo "$MODEL ($PRODUCT)"

METADATA_DOWNLOAD_SIZE="16384"
OTA_METADATA_URL="$(echo "$OTA_LIST" | grep "$PRODUCT")"

if command -v curl > /dev/null 2>&1; then
  download "$OTA_METADATA_URL" PIXEL_ZIP_METADATA "$METADATA_DOWNLOAD_SIZE"
else
  ULIMIT_BLOCKS=$(( (METADATA_DOWNLOAD_SIZE + 511) / 512 ))
  (ulimit -f "$ULIMIT_BLOCKS"; busybox wget -T 10 --no-check-certificate -qO - "$OTA_METADATA_URL" > PIXEL_ZIP_METADATA) >/dev/null 2>&1
fi

FINGERPRINT="$(strings PIXEL_ZIP_METADATA 2>/dev/null | grep -am1 'post-build=' | cut -d= -f2)"
SECURITY_PATCH="$(strings PIXEL_ZIP_METADATA 2>/dev/null | grep -am1 'security-patch-level=' | cut -d= -f2)"

if [ -z "$FINGERPRINT" ] || [ -z "$SECURITY_PATCH" ]; then
	rm -f PIXEL_ZIP_METADATA
	download_fail "https://dl.google.com"
fi

echo "- Dumping values to pif.json ..."
cat <<EOF | tee pif.json
{
  "FINGERPRINT": "$FINGERPRINT",
  "MANUFACTURER": "Google",
  "MODEL": "$MODEL",
  "SECURITY_PATCH": "$SECURITY_PATCH"
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
