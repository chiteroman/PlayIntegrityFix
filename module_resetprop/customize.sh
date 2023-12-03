# Error on < Android 8.
if [ "$API" -lt 26 ]; then
    abort "!!! You can't use this module on Android < 8.0"
fi

# Check custom pif.json
if [ -e /data/adb/pif.json ]; then
    ui_print "!!! WARNING, You are using a custom pif.json. Module will use it."
	ui_print "!!! If you want to use recommended one, remove this file."
	ui_print "!!! To remove it, execute as root: rm -f /data/adb/pif.json"
fi

# SafetyNet-Fix module is obsolete and it's incompatible with PIF.
if [ -d /data/adb/modules/safetynet-fix ]; then
    touch /data/adb/modules/safetynet-fix/remove
    ui_print "!!! SafetyNet-Fix module will be removed on next reboot."
fi

# MagiskHidePropsConf module is obsolete in Android 8+ but it shouldn't give issues.
if [ -d /data/adb/modules/MagiskHidePropsConf ]; then
    ui_print "!!! WARNING, MagiskHidePropsConf module may cause issues with PIF"
fi

# curl & resetprop
mv -f $MODPATH/bin/$ABI/curl $MODPATH
mv -f $MODPATH/bin/$ABI/resetprop $MODPATH

rm -rf $MODPATH/bin

set_perm $MODPATH/curl root root 777
set_perm $MODPATH/resetprop root root 777
