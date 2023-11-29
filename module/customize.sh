# Error on < Android 8
if [ "$API" -lt 26 ]; then
    abort "!!! You can't use this module on Android < 8.0"
fi

# Remove safetynet-fix module if installed
if [ -d /data/adb/modules/safetynet-fix ]; then
    touch /data/adb/modules/safetynet-fix/remove
    ui_print "- 'safetynet-fix' module will be removed on next reboot"
fi

# Copy any custom.pif.json to updated module
if [ -f /data/adb/modules/playintegrityfix/custom.pif.json ]; then
    ui_print "- Restoring custom.pif.json"
    cp -af /data/adb/modules/playintegrityfix/custom.pif.json $MODPATH/custom.pif.json
fi

# Clean up any leftover files from previous deprecated methods
rm -f /data/data/com.google.android.gms/cache/pif.prop /data/data/com.google.android.gms/pif.prop
rm -f /data/data/com.google.android.gms/cache/pif.json /data/data/com.google.android.gms/pif.json
