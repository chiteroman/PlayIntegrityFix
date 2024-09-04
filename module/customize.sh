# Module requires Zygisk to work
if [ "$ZYGISK_ENABLED" != "1" ] && [ ! -d "/data/adb/modules/zygisksu" ]; then
    abort "! Zygisk is not enabled. Please, enable Zygisk in Magisk Settings or install the ZygiskNext or ReZygisk module."
fi

# Error on < Android 8
if [ "$API" -lt 26 ]; then
    abort "! You can't use this module on Android < 8.0"
fi

# safetynet-fix module is obsolete and it's incompatible with PIF
if [ -d "/data/adb/modules/safetynet-fix" ]; then
    touch "/data/adb/modules/safetynet-fix/remove"
    ui_print "! safetynet-fix module removed. Do NOT install it again along PIF"
fi

# playcurl must be removed when flashing PIF
if [ -d "/data/adb/modules/playcurl" ]; then
    touch "/data/adb/modules/playcurl/remove"
    ui_print "! playcurl module removed!"
fi

# MagiskHidePropsConf module is obsolete in Android 8+ but it shouldn't give issues
if [ -d "/data/adb/modules/MagiskHidePropsConf" ]; then
    ui_print "! WARNING, MagiskHidePropsConf module may cause issues with PIF."
fi

# Check custom fingerprint
if [ -f "/data/adb/pif.json" ]; then
    mv -f "/data/adb/pif.json" "/data/adb/pif.json.old"
    ui_print "- Backup custom pif.json"
fi
