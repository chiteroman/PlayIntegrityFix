# Error on < Android 8
if [ "$API" -lt 26 ]; then
    abort "!!! You can't use this module on Android < 8.0."
fi

# safetynet-fix module is incompatible
if [ -d "/data/adb/modules/safetynet-fix" ]; then
    touch "/data/adb/modules/safetynet-fix/remove"
	ui_print "- 'safetynet-fix' module will be removed in next reboot."
fi

# Use custom resetprop only in Android 10+
if [ "$API" -gt 28 ]; then
	mv -f "$MODPATH/bin/$ABI/resetprop" "$MODPATH"
	ui_print "- Using custom resetprop to avoid detections."
fi

rm -rf "$MODPATH/bin"