#!/system/bin/sh

if [ "$USER" != "root" -o "$(whoami 2>/dev/null)" != "root" ]; then
  echo "autopif: need root permissions";
  exit 1;
fi;

case "$1" in
  -h|--help|help) echo "sh autopif.sh [-a]"; exit 0;;
  -a|--advanced|advanced) ARGS="-a";;
esac;

echo "Xiaomi.eu pif.json extractor script \
  \n  by osm0sis @ xda-developers";

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

if [ "$DIR" = /data/adb/modules/playintegrityfix ]; then
  DIR=$DIR/autopif;
  mkdir -p $DIR;
fi;
cd "$DIR";

if [ ! -f apktool_2.0.3-dexed.jar ]; then
  item "Downloading Apktool ...";
  wget --no-check-certificate -O apktool_2.0.3-dexed.jar https://github.com/osm0sis/APK-Patcher/raw/master/tools/apktool_2.0.3-dexed.jar 2>&1 || exit 1;
fi;

item "Finding latest APK from RSS feed ...";
APKURL=$(wget -q -O - --no-check-certificate https://sourceforge.net/projects/xiaomi-eu-multilang-miui-roms/rss?path=/xiaomi.eu/Xiaomi.eu-app | grep -o '<link>.*' | head -n 2 | tail -n 1 | sed 's;<link>\(.*\)</link>;\1;g');
APKNAME=$(echo $APKURL | sed 's;.*/\(.*\)/download;\1;g');
echo "$APKNAME";

if [ ! -f $APKNAME ]; then
  item "Downloading $APKNAME ...";
  wget --no-check-certificate -O $APKNAME $APKURL 2>&1 || exit 1;
fi;

OUT=$(basename $APKNAME .apk);
if [ ! -d $OUT ]; then
  item "Extracting APK files with Apktool ...";
  DALVIKVM=dalvikvm;
  if echo "$PREFIX" | grep -q "termux"; then
    if [ "$TERMUX_VERSION" ]; then
      if grep -q "apex" $PREFIX/bin/dalvikvm; then
        DALVIKVM=$PREFIX/bin/dalvikvm;
      else
        die 'Outdated Termux packages, run "pkg upgrade" from a user prompt';
      fi;
    else
      die "Play Store Termux not supported, use GitHub/F-Droid Termux";
    fi;
  fi;
  $DALVIKVM -Xnoimage-dex2oat -cp apktool_2.0.3-dexed.jar brut.apktool.Main d -f --no-src -p $OUT -o $OUT $APKNAME || exit 1;
  [ -f $OUT/res/xml/inject_fields.xml ] || die "inject_fields.xml not found";
fi;

item "Converting inject_fields.xml to pif.json ...";
(echo '{';
grep -o '<field.*' $OUT/res/xml/inject_fields.xml | sed 's;.*name=\(".*"\) type.* value=\(".*"\).*;  \1: \2,;g';
echo '  "DEVICE_INITIAL_SDK_INT": "32",' ) | sed '$s/,/\n}/' | tee pif.json;
grep -q "FINGERPRINT" pif.json || die "Failed to extract information from inject_fields.xml";

for MIGRATE in migrate.sh /data/adb/modules/playintegrityfix/migrate.sh; do
  [ -f "$MIGRATE" ] && break; 
done;
if [ -f "$MIGRATE" ]; then
  OLDJSON=/data/adb/modules/playintegrityfix/custom.pif.json;
  [ -f "$OLDJSON" ] && grep -qE "verboseLogs|VERBOSE_LOGS" $OLDJSON && ARGS="-a";
  item "Converting pif.json to custom.pif.json with migrate.sh:";
  rm -f custom.pif.json;
  sh $MIGRATE -i $ARGS pif.json;
  if [ -n "$ARGS" ]; then
    grep_json() { grep -m1 "$1" $2 | cut -d\" -f4; }
    verboseLogs=$(grep_json "VERBOSE_LOGS" $OLDJSON);
    ADVSETTINGS="spoofBuild spoofProps spoofProvider spoofSignature verboseLogs";
    for SETTING in $ADVSETTINGS; do
      eval [ -z \"\$$SETTING\" ] \&\& $SETTING=$(grep_json "$SETTING" $OLDJSON);
      eval TMPVAL=\$$SETTING;
      [ -n "$TMPVAL" ] && sed -i "s;\($SETTING\": \"\).;\1$TMPVAL;" custom.pif.json;
    done;
  fi;
  grep -q '//"\*.security_patch"' $OLDJSON && sed -i 's;"\*.security_patch";//"\*.security_patch";' custom.pif.json;
  cat custom.pif.json;
fi;

if [ "$DIR" = /data/adb/modules/playintegrityfix/autopif ]; then
  if [ -f /data/adb/modules/playintegrityfix/migrate.sh ]; then
    NEWNAME="custom.pif.json";
  else
    NEWNAME="pif.json";
  fi;
  if [ -f "../$NEWNAME" ]; then
    item "Renaming old file to $NEWNAME.bak ...";
    mv -fv ../$NEWNAME ../$NEWNAME.bak;
  fi;
  item "Installing new json ...";
  cp -fv $NEWNAME ..;
  if [ -f /data/adb/modules/playintegrityfix/killgms.sh ]; then
    item "Killing any running GMS DroidGuard process ...";
    sh /data/adb/modules/playintegrityfix/killgms.sh 2>&1 || true;
  fi;
fi;
