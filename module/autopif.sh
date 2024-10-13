#!/system/bin/sh

case "$0" in
  *.sh) DIR="$0";;
  *) DIR="$(lsof -p $$ 2>/dev/null | grep -o '/.*autopif.sh$')";;
esac;
DIR=$(dirname "$(readlink -f "$DIR")");

item() { echo "\n- $@"; }
die() { echo "\nError: $@!"; exit 1; }

find_busybox() {
  [ -n "$BUSYBOX" ] && return 0;
  local path;
  for path in /data/adb/modules/busybox-ndk/system/*/busybox /data/adb/magisk/busybox /data/adb/ksu/bin/busybox /data/adb/ap/bin/busybox; do
    if [ -f "$path" ]; then
      BUSYBOX="$path";
      return 0;
    fi;
  done;
  return 1;
}

if ! which wget >/dev/null || grep -q "wget-curl" $(which wget); then
  if ! find_busybox; then
    die "wget not found, install busybox";
  elif $BUSYBOX ping -c1 -s2 android.com 2>&1 | grep -q "bad address"; then
    die "wget broken, install busybox";
  else
    wget() { $BUSYBOX wget "$@"; }
  fi;
fi;

rm -f pif.json

if [ "$DIR" = /data/adb/modules/playintegrityfix ]; then
  DIR=$DIR/autopif;
  mkdir -p $DIR;
fi;
cd "$DIR";

wget -q -O PIXEL_GSI_HTML --no-check-certificate https://developer.android.com/topic/generic-system-image/releases 2>&1 || exit 1;
grep -m1 -o 'li>.*(Beta)' PIXEL_GSI_HTML | cut -d\> -f2;

BETA_REL_DATE="$(date -D '%B %e, %Y' -d "$(grep -m1 -o 'Date:.*' PIXEL_GSI_HTML | cut -d\  -f2-4)" '+%Y-%m-%d')";
BETA_EXP_DATE="$(date -D '%s' -d "$(($(date -D '%Y-%m-%d' -d "$BETA_REL_DATE" '+%s') + 60 * 60 * 24 * 7 * 6))" '+%Y-%m-%d')";
echo "Beta Released: $BETA_REL_DATE \
  \nEstimated Expiry: $BETA_EXP_DATE";

RELEASE="$(grep -m1 'corresponding Google Pixel builds' PIXEL_GSI_HTML | grep -o '/versions/.*' | cut -d\/ -f3)";
ID="$(grep -m1 -o 'Build:.*' PIXEL_GSI_HTML | cut -d\  -f2)";
INCREMENTAL="$(grep -m1 -o "$ID-.*-" PIXEL_GSI_HTML | cut -d- -f2)";

wget -q -O PIXEL_GET_HTML --no-check-certificate https://developer.android.com$(grep -m1 'corresponding Google Pixel builds' PIXEL_GSI_HTML | grep -o 'href.*' | cut -d\" -f2) 2>&1 || exit 1;
wget -q -O PIXEL_BETA_HTML --no-check-certificate https://developer.android.com$(grep -m1 'Factory images for Google Pixel' PIXEL_GET_HTML | grep -o 'href.*' | cut -d\" -f2) 2>&1 || exit 1;

MODEL_LIST="$(grep -A1 'tr id=' PIXEL_BETA_HTML | grep 'td' | sed 's;.*<td>\(.*\)</td>;\1;')";
PRODUCT_LIST="$(grep -o 'factory/.*_beta' PIXEL_BETA_HTML | cut -d\/ -f2)";

wget -q -O PIXEL_SECBULL_HTML --no-check-certificate https://source.android.com/docs/security/bulletin/pixel 2>&1 || exit 1;

SECURITY_PATCH="$(grep -A15 "$(grep -m1 -o 'Security patch level:.*' PIXEL_GSI_HTML | cut -d\  -f4-)" PIXEL_SECBULL_HTML | grep -m1 -B1 '</tr>' | grep 'td' | sed 's;.*<td>\(.*\)</td>;\1;')";

case "$1" in
  -m)
    DEVICE="$(getprop ro.product.device)";
    case "$PRODUCT_LIST" in
      *${DEVICE}_beta*)
        MODEL="$(getprop ro.product.model)";
        PRODUCT="${DEVICE}_beta";
      ;;
    esac;
  ;;
esac;
item "Selecting Pixel Beta device ...";
if [ -z "$PRODUCT" ]; then
  set_random_beta() {
    local list_count="$(echo "$MODEL_LIST" | wc -l)";
    local list_rand="$((RANDOM % $list_count + 1))";
    local IFS=$'\n';
    set -- $MODEL_LIST;
    MODEL="$(eval echo \${$list_rand})";
    set -- $PRODUCT_LIST;
    PRODUCT="$(eval echo \${$list_rand})";
    DEVICE="$(echo "$PRODUCT" | sed 's/_beta//')";
  }
  set_random_beta;
fi;
echo "$MODEL ($PRODUCT)";

item "Dumping values to minimal pif.json ...";
cat <<EOF | tee pif.json;
{
  "MANUFACTURER": "Google",
  "MODEL": "$MODEL",
  "FINGERPRINT": "google/$PRODUCT/$DEVICE:$RELEASE/$ID/$INCREMENTAL:user/release-keys",
  "PRODUCT": "$PRODUCT",
  "DEVICE": "$DEVICE",
  "SECURITY_PATCH": "$SECURITY_PATCH",
  "DEVICE_INITIAL_SDK_INT": 32
}
EOF

JSON=/data/adb/modules/playintegrityfix/pif.json
ADVSETTINGS="spoofBuild spoofProps spoofProvider spoofSignature";
for SETTING in $ADVSETTINGS; do
  eval [ -z \"\$$SETTING\" ] \&\& $SETTING=$(grep_json "$SETTING" $JSON);
  eval TMPVAL=\$$SETTING;
  [ -n "$TMPVAL" ] && sed -i "s;\($SETTING\": \"\).;\1$TMPVAL;" pif.json;
done;

grep -q '//"\*.security_patch"' $JSON && sed -i 's;"\*.security_patch";//"\*.security_patch";' pif.json;
  sed -i "s;};\n  // Beta Released: $BETA_REL_DATE\n  // Estimated Expiry: $BETA_EXP_DATE\n};" pif.json;
  cat pif.json;
  
cp $DIR/pif.json /data/adb/modules/playintegrityfix
DIR = /data/adb/modules/playintegrityfix
cd $DIR

N="
";

item() { echo "- $@"; }
die() { [ "$INSTALL" ] || echo "$N$N! $@"; exit 1; }
grep_get_json() {
  local target="$FILE";
  [ -n "$2" ] && target="$2";
  eval set -- "$(cat "$target" | tr -d '\r\n' | grep -m1 -o "$1"'".*' | cut -d: -f2-)";
  echo "$1" | sed -e 's|"|\\\\\\"|g' -e 's|[,}]*$||';
}
grep_check_json() {
  local target="$FILE";
  [ -n "$2" ] && target="$2";
  grep -q "$1" "$target" && [ "$(grep_get_json $1 "$target")" ];
}

until [ -z "$1" -o -f "$1" ]; do
  case "$1" in
    -f|--force|force) FORCE=1; shift;;
    -o|--override|override) OVERRIDE=1; shift;;
    -a|--advanced|advanced) ADVANCED=1; shift;;
    *) die "Invalid argument/file not found: $1";;
  esac;
done;

if [ -f "$1" ]; then
  FILE="$1";
  DIR="$1";
else
  case "$0" in
    *.sh) DIR="$0";;
    *) DIR="$(lsof -p $$ 2>/dev/null | grep -o '/.*migrate.sh$')";;
  esac;
fi;
DIR=$(dirname "$(readlink -f "$DIR")");
[ -z "$FILE" ] && FILE="$DIR/pif.json";

OUT="$2";
[ -z "$OUT" ] && OUT="$DIR/pif.json";

[ -f "$FILE" ] || die "No json file found";

grep_check_json api_level && [ ! "$FORCE" ] && die "No migration required";

[ "$INSTALL" ] || item "Parsing fields ...";

FPFIELDS="BRAND PRODUCT DEVICE RELEASE ID INCREMENTAL TYPE TAGS";
ALLFIELDS="MANUFACTURER MODEL FINGERPRINT $FPFIELDS SECURITY_PATCH DEVICE_INITIAL_SDK_INT";

for FIELD in $ALLFIELDS; do
  eval $FIELD=\"$(grep_get_json $FIELD)\";
done;

if [ -n "$ID" ] && ! grep_check_json build.id; then
  item 'Simple entry ID found, changing to ID field and "*.build.id" property ...';
fi;

if [ -z "$ID" ] && grep_check_json BUILD_ID; then
  item 'Deprecated entry BUILD_ID found, changing to ID field and "*.build.id" property ...';
  ID="$(grep_get_json BUILD_ID)";
fi;

if [ -n "$SECURITY_PATCH" ] && ! grep_check_json security_patch; then
  item 'Simple entry SECURITY_PATCH found, changing to SECURITY_PATCH field and "*.security_patch" property ...';
fi;

if grep_check_json VNDK_VERSION; then
  item 'Deprecated entry VNDK_VERSION found, changing to "*.vndk.version" property ...';
  VNDK_VERSION="$(grep_get_json VNDK_VERSION)";
fi;

# Get the device's SDK version
sdk_version=$(getprop ro.build.version.sdk)

# Print the SDK version
echo "Device SDK version: $sdk_version"

if [ -n "$DEVICE_INITIAL_SDK_INT" ] && ! grep_check_json api_level; then
  item 'Simple entry DEVICE_INITIAL_SDK_INT found, changing to DEVICE_INITIAL_SDK_INT field and "*api_level" property ...';
fi;

if [ -z "$DEVICE_INITIAL_SDK_INT" ] && grep_check_json FIRST_API_LEVEL; then
  item 'Deprecated entry FIRST_API_LEVEL found, changing to DEVICE_INITIAL_SDK_INT field and "*api_level" property ...';
  DEVICE_INITIAL_SDK_INT="$(grep_get_json FIRST_API_LEVEL)";
fi;

if [ -z "$RELEASE" -o -z "$INCREMENTAL" -o -z "$TYPE" -o -z "$TAGS" -o "$OVERRIDE" ]; then
  if [ "$OVERRIDE" ]; then
    item "Overriding values for fields derivable from FINGERPRINT ...";
  else
    item "Missing default fields found, deriving from FINGERPRINT ...";
  fi;
  IFS='/:' read F1 F2 F3 F4 F5 F6 F7 F8 <<EOF
$(grep_get_json FINGERPRINT)
EOF
  i=1;
  for FIELD in $FPFIELDS; do
    eval [ -z \"\$$FIELD\" -o \"$OVERRIDE\" ] \&\& $FIELD=\"\$F$i\";
    i=$((i+1));
  done;
fi;

if [ -z "$SECURITY_PATCH" -o "$SECURITY_PATCH" = "null" ]; then
  item 'Missing required SECURITY_PATCH field and "*.security_patch" property value found, leaving empty ...';
  unset SECURITY_PATCH;
fi;

if [ -z "$DEVICE_INITIAL_SDK_INT" -o "$DEVICE_INITIAL_SDK_INT" = "null" ]; then
  item 'Missing required DEVICE_INITIAL_SDK_INT field and "*api_level" property value found, setting to 25 ...';
  DEVICE_INITIAL_SDK_INT=25;
fi;

ALLFIELDS="TYPE TAGS ID BRAND DEVICE FINGERPRINT MANUFACTURER MODEL PRODUCT INCREMENTAL RELEASE SECURITY_PATCH";

spoofProps=true;
spoofProvider=true;
spoofSignature=false;
DEBUG=false;

if echo "$get_keys" | "$busybox_path" grep -q test; then
    spoofSignature=true;
fi

[ "$INSTALL" ] || item "Writing fields and properties to updated pif.json ...";

(echo "{";
for FIELD in $ALLFIELDS; do
  eval echo '\ \ \"$FIELD\": \"'\$$FIELD'\",';
done;
echo '  "DEVICE_INITIAL_SDK_INT": '$DEVICE_INITIAL_SDK_INT',';
echo '  "spoofProvider": '$spoofProvider',';
echo '  "spoofProps": '$spoofProps',';
echo '  "spoofSignature": '$spoofSignature',';
echo '  "DEBUG": '$DEBUG'';
echo "}"
) | sed '$s/,/\n}/' > "$OUT";

[ "$INSTALL" ] || cat "$OUT";

MODDIR=${0%/*}

# Find and remove directories containing "xiaomi" in their name
echo Cleaning up...
find "$MODDIR" -type d -name '*autopif*' | while read -r dir; do
  rm -rf "$dir"
  
su -c killall com.google.android.gms.unstable
done