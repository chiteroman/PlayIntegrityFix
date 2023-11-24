# Error on < Android 8
if [ "$API" -lt 26 ]; then
    abort "!!! You can't use this module on Android < 8.0."
fi

# safetynet-fix module is incompatible
if [ -d "/data/adb/modules/safetynet-fix" ]; then
    ui_print "!!! safetynet-fix module removed!"
    touch "/data/adb/modules/safetynet-fix/remove"
fi

# Backup old pif.prop
if [ -e "/data/adb/modules/playintegrityfix/pif.prop" ]; then
	ui_print "- Backup old pif.prop."
    mv "/data/adb/modules/playintegrityfix/pif.prop" "/data/adb/pif.prop.old"
fi

# use our resetprop
mv -f "$MODPATH/bin/$ABI/resetprop" "$MODPATH"
rm -rf "$MODPATH/bin"