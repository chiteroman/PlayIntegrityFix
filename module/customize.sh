# Error on < Android 8.
if [ "$API" -lt 26 ]; then
    abort "- !!! You can't use this module on Android < 8.0"
fi

# safetynet-fix module is obsolete and it's incompatible with PIF.
if [ -d /data/adb/modules/safetynet-fix ]; then
    abort "- !!! REMOVE safetynet-fix module and do NOT install it again along PIF."
fi

# Check pif.json
if [ ! -f /data/adb/pif.json ]; then
    mv -f $MODPATH/pif.json /data/adb/pif.json
    ui_print "- Moved pif.json template, you must modify it."
fi

rm -f $MODPATH/pif.json

# MagiskHidePropsConf module is obsolete in Android 8  but it shouldn't give issues.
if [ -d /data/adb/modules/MagiskHidePropsConf ]; then
    ui_print "- ! WARNING, MagiskHidePropsConf module may cause issues with PIF"
fi

# Check for xiaomi.eu apps

if [ -d "/product/app/XiaomiEUInject" ]; then

    directory="$MODPATH/product/app/XiaomiEUInject"

    [ -d "$directory" ] || mkdir -p "$directory"

    touch "$directory/.replace"

    ui_print "- XiaomiEUInject app removed."
fi

if [ -d "/system/app/XInjectModule" ]; then

    directory="$MODPATH/system/app/XInjectModule"

    [ -d "$directory" ] || mkdir -p "$directory"

    touch "$directory/.replace"

    ui_print "- XInjectModule app removed."
fi