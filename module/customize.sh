# Error on < Android 8.
if [ "$API" -lt 26 ]; then
    abort "- !!! You can't use this module on Android < 8.0"
fi

# safetynet-fix module is obsolete and it's incompatible with PIF.
if [ -d /data/adb/modules/safetynet-fix ]; then
    rm -rf /data/adb/modules/safetynet-fix
    rm -f /data/adb/SNFix.dex
    ui_print "! safetynet-fix module will be removed. Do NOT install it again along PIF."
fi

# MagiskHidePropsConf module is obsolete in Android 8+ but it shouldn't give issues.
if [ -d /data/adb/modules/MagiskHidePropsConf ]; then
    ui_print "! WARNING, MagiskHidePropsConf module may cause issues with PIF."
fi

# Remove xiaomi.eu apps
if [ -d "/product/app/XiaomiEUInject" ]; then
    directory="$MODPATH/product/app/XiaomiEUInject"
    [ -d "$directory" ] || mkdir -p "$directory"
    touch "$directory/.replace"
    ui_print "- XiaomiEUInject app removed."
fi

# Remove EliteRoms app
if [ -d "/system/app/XInjectModule" ]; then
    directory="$MODPATH/system/app/XInjectModule"
    [ -d "$directory" ] || mkdir -p "$directory"
    touch "$directory/.replace"
    ui_print "- XInjectModule app removed."
fi
if [ -d "/system/app/EliteDevelopmentModule" ]; then
    directory="$MODPATH/system/app/EliteDevelopmentModule"
    [ -d "$directory" ] || mkdir -p "$directory"
    touch "$directory/.replace"
    ui_print "- EliteDevelopmentModule app removed."
fi

# Copy default pif.json if it doesn't exist.
if [ ! -e /data/adb/pif.json ]; then
	cp -af $MODPATH/pif.json /data/adb/pif.json
	ui_print "- Moved default pif.json file."
fi

# Copy default pif.json over the old one.
if [ ! -d /data/adb/pif.json ]; then
	cp -af $MODPATH/pif.json /data/adb/pif.json
	ui_print "- Applied new pif.json file."
fi
