# Don't flash in recovery!
if ! $BOOTMODE; then
    ui_print "*********************************************************"
    ui_print "! Install from recovery is NOT supported"
    ui_print "! Recovery sucks"
    ui_print "! Please install from Magisk / KernelSU / APatch app"
    abort    "*********************************************************"
fi

# Module requires Zygisk to work
isZygiskEnabled=$(magisk --sqlite "SELECT value FROM settings WHERE key='zygisk';")
if [ "$isZygiskEnabled" == "value=0" ] && [ ! -d "/data/adb/modules/zygisksu" ]; then
    abort "! Zygisk is not enabled. Please, enable Zygisk in Magisk settings or install ZygiskNext or ReZygisk module."
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

# playcurl warn
if [ -d "/data/adb/modules/playcurl" ]; then
    ui_print "! playcurl may overwrite fingerprint with invalid one, be careful!"
fi

# MagiskHidePropsConf module is obsolete in Android 8+ but it shouldn't give issues
if [ -d "/data/adb/modules/MagiskHidePropsConf" ]; then
    ui_print "! WARNING, MagiskHidePropsConf module may cause issues with PIF."
fi

# Check custom fingerprint
if [ -f "/data/adb/pif.json" ]; then
    ui_print "!!! WARNING !!!"
    ui_print "- You are using custom pif.json (/data/adb/pif.json)"
    ui_print "- Remove that file if you can't pass attestation test!"
fi

# Uninstall conflict apps
APPS="
/system/app/EliteDevelopmentModule
/system/app/XInjectModule
/system/product/app/XiaomiEUInject
/system/product/app/XiaomiEUInject-Stub
/system/system_ext/app/hentaiLewdbSVTDummy
/system/system_ext/app/PifPrebuilt
"

for APP in $APPS; do
    if [ -d "$APP" ]; then
        mkdir -p "${MODPATH}${APP}"
        if [ "$KSU" = true ] || [ "$APATCH" = true ]; then
            mknod "${MODPATH}${APP}" c 0 0
        else
            touch "${MODPATH}${APP}/.replace"
        fi
        ui_print "- Removed: $(basename "$APP")"
    fi
done
