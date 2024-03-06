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

# Check custom fingerprint
if [ -f "/data/adb/pif.json" ]; then
    ui_print "- You are using custom fingerprint!"
    ui_print "- If you fail DEVICE verdict, remove /data/adb/pif.json file"
    ui_print "- If pif.json file doesn't exist, module will use default one"
fi

ui_print "- Removing conflict apps..."

# Remove conflict apps
APPS="
/system/app/EliteDevelopmentModule
/system/app/XInjectModule
/system/product/app/XiaomiEUInject
/system/product/app/XiaomiEUInject-Stub
/system/system_ext/app/hentaiLewdbSVTDummy
/system/system_ext/app/PifPrebuilt
"

for app in $APPS; do
        if [ -d "$app" ]; then
            directory="$MODPATH$app"
            [ -d "$directory" ] || mkdir -p "$directory"
            if [ "$KSU" = "true" ] || [ "$APATCH" = "true" ]; then
                mknod $directory c 0 0
            else
                touch $directory/.replace
            fi
            ui_print "- ${app##*/} app removed"
        else
            ui_print "- ${app##*/} app doesn't exist, skip"
        fi
done
