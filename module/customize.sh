# Android < 8.0
if [ "$API" -lt 26 ]; then
    abort "!!! You can't use this module on Android < 8.0"
fi

# Check if safetynet-fix is installed
if [ -d "/data/adb/modules/safetynet-fix" ]; then
	ui_print "! safetynet-fix module will be removed"
    touch "/data/adb/modules/safetynet-fix/remove"
fi