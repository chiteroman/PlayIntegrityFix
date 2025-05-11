#!/bin/sh

PATH=/data/adb/ap/bin:/data/adb/ksu/bin:/data/adb/magisk:/data/data/com.termux/files/usr/bin:$PATH
MODDIR=/data/adb/modules/playintegrityfix
version=$(grep "^version=" $MODDIR/module.prop | sed 's/version=//g')
FORCE_PREVIEW=1

# lets try to use tmpfs for processing
TEMPDIR="$MODDIR/temp" #fallback
[ -w /sbin ] && TEMPDIR="/sbin/playintegrityfix"
[ -w /debug_ramdisk ] && TEMPDIR="/debug_ramdisk/playintegrityfix"
[ -w /dev ] && TEMPDIR="/dev/playintegrityfix"
mkdir -p "$TEMPDIR"
cd "$TEMPDIR"

echo "[+] PlayIntegrityFix $version"
echo "[+] $(basename "$0")"
printf "\n\n"

sleep_pause() {
	# APatch and KernelSU needs this
	# but not KSU_NEXT, MMRL
	if [ -z "$MMRL" ] && [ -z "$KSU_NEXT" ] && { [ "$KSU" = "true" ] || [ "$APATCH" = "true" ]; }; then
		sleep 5
	fi
}

download_fail() {
	dl_domain=$(echo "$1" | awk -F[/:] '{print $4}')
	# If downloading OTA metadata (a .zip URL) fails due to size limit, don't exit immediately.
	# Allow the script to try parsing what was downloaded.
	# If parsing fails (fingerprint is empty), then a generic error will be shown.
	echo "$1" | grep -q "\.zip$" && return

	# Clean up on download fail
	rm -rf "$TEMPDIR"
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

# $1: URL, $2: output_file, $3: optional_max_bytes (e.g., "16384")
download() {
  local url="$1"
  local output_file="$2"
  local max_bytes="$3"
  
  if [ -n "$max_bytes" ]; then
    busybox wget --quota="$max_bytes" -T 10 --no-check-certificate -qO - "$url" > "$output_file" || download_fail "$url"
  else
    busybox wget -T 10 --no-check-certificate -qO - "$url" > "$output_file" || download_fail "$url"
  fi
}

if command -v curl > /dev/null 2>&1; then
  download() {
    local url="$1"
    local output_file="$2"
    local max_bytes="$3" # curl --max-filesize expects bytes as a number
    
    if [ -n "$max_bytes" ]; then
      curl --connect-timeout 10 -s -L --max-filesize "$max_bytes" "$url" > "$output_file" || download_fail "$url"
    else
      curl --connect-timeout 10 -s -L "$url" > "$output_file" || download_fail "$url"
    fi
  }
fi

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
}

# Get latest Pixel Beta information
download https://developer.android.com/about/versions PIXEL_VERSIONS_HTML
BETA_URL=$(grep -o 'https://developer.android.com/about/versions/.*[0-9]"' PIXEL_VERSIONS_HTML | sort -ru | cut -d\" -f1 | head -n1)
download "$BETA_URL" PIXEL_LATEST_HTML

# Handle Developer Preview vs Beta
if grep -qE 'Developer Preview|tooltip>.*preview program' PIXEL_LATEST_HTML && [ "$FORCE_PREVIEW" = 0 ]; then
	# Use the second latest version for beta
	BETA_URL=$(grep -o 'https://developer.android.com/about/versions/.*[0-9]"' PIXEL_VERSIONS_HTML | sort -ru | cut -d\" -f1 | head -n2 | tail -n1)
	download "$BETA_URL" PIXEL_BETA_HTML
else
	mv -f PIXEL_LATEST_HTML PIXEL_BETA_HTML
fi

# Get OTA information
OTA_URL="https://developer.android.com$(grep -o 'href=".*download-ota.*"' PIXEL_BETA_HTML | cut -d\" -f2 | head -n1)"
download "$OTA_URL" PIXEL_OTA_HTML

# Extract device information
MODEL_LIST="$(grep -A1 'tr id=' PIXEL_OTA_HTML | grep 'td' | sed 's;.*<td>\(.*\)</td>;\1;')"
PRODUCT_LIST="$(grep -o 'ota/.*_beta' PIXEL_OTA_HTML | cut -d\/ -f2)"
OTA_LIST="$(grep 'ota/.*_beta' PIXEL_OTA_HTML | cut -d\" -f2)"

# Select and configure device
echo "- Selecting Pixel Beta device ..."
[ -z "$PRODUCT" ] && set_random_beta
echo "$MODEL ($PRODUCT)"

# Get device fingerprint and security patch from OTA metadata (downloading only first 16KB)
METADATA_DOWNLOAD_SIZE="16384" # Bytes (16KB)
download "$(echo "$OTA_LIST" | grep "$PRODUCT")" PIXEL_ZIP_METADATA "$METADATA_DOWNLOAD_SIZE"
FINGERPRINT="$(strings PIXEL_ZIP_METADATA | grep -am1 'post-build=' | cut -d= -f2)"
SECURITY_PATCH="$(strings PIXEL_ZIP_METADATA | grep -am1 'security-patch-level=' | cut -d= -f2)"

# Validate required field to prevent empty pif.json
if [ -z "$FINGERPRINT" ] || [ -z "$SECURITY_PATCH" ]; then
	# This call to download_fail will use a non-.zip URL, so it will exit if it fails.
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
