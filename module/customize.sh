# Error on < Android 8
if [ "$API" -lt 26 ]; then
    abort "- !!! You can't use this module on Android < 8.0"
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

REMOVE="
/system/product/app/XiaomiEUInject
/system/product/app/XiaomiEUInject-Stub
/system/system/app/EliteDevelopmentModule
/system/system/app/XInjectModule
/system/system_ext/app/hentaiLewdbSVTDummy
/system/system_ext/app/PifPrebuilt
/system/system_ext/overlay/CertifiedPropsOverlay.apk
"

if [ "$KSU" = "true" -o "$APATCH" = "true" ]; then
    ui_print "- KernelSU/APatch detected, conflicting apps will be automatically removed"
else
    ui_print "- Magisk detected, removing conflicting apps one by one :("
    echo "$REMOVE" | grep -v '^$' | while read -r line; do
        if [ -d "$line" ]; then
            mkdir -p "${MODPATH}${line}"
            touch "${MODPATH}${line}/.replace"
            ui_print "- Removed dir: $line"
        elif [ -f "$line" ]; then
            dir=$(dirname "$line")
            filename=$(basename "$line")
            mkdir -p "${MODPATH}${dir}"
            touch "${MODPATH}${dir}/${filename}"
            ui_print "- Removed file: $line"
        fi
    done
fi
