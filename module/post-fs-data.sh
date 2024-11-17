MODPATH="${0%/*}"
. "$MODPATH"/common_func.sh

# Remove Play Services from Magisk DenyList when set to Enforce in normal mode
if magisk --denylist status; then
    magisk --denylist rm com.google.android.gms
else
    # Check if Shamiko is installed and whitelist feature isn't enabled
    if [ -d "/data/adb/modules/zygisk_shamiko" ] && [ ! -f "/data/adb/shamiko/whitelist" ]; then
        magisk --denylist add com.google.android.gms com.google.android.gms
        magisk --denylist add com.google.android.gms com.google.android.gms.unstable
    fi
fi

# Conditional early sensitive properties

# Samsung
resetprop_if_diff ro.boot.warranty_bit 0
resetprop_if_diff ro.vendor.boot.warranty_bit 0
resetprop_if_diff ro.vendor.warranty_bit 0
resetprop_if_diff ro.warranty_bit 0

# Realme
resetprop_if_diff ro.boot.realmebootstate green

# OnePlus
resetprop_if_diff ro.is_ever_orange 0

# Microsoft
for PROP in $(resetprop | grep -oE 'ro.*.build.tags'); do
    resetprop_if_diff "$PROP" release-keys
done

# Other
for PROP in $(resetprop | grep -oE 'ro.*.build.type'); do
    resetprop_if_diff "$PROP" user
done
resetprop_if_diff ro.adb.secure 1
resetprop_if_diff ro.debuggable 0
resetprop_if_diff ro.force.debuggable 0
resetprop_if_diff ro.secure 1

# Work around AOSPA PropImitationHooks conflict when their persist props don't exist
if [ -n "$(resetprop ro.aospa.version)" ]; then
    for PROP in persist.sys.pihooks.first_api_level persist.sys.pihooks.security_patch; do
        resetprop | grep -q "\[$PROP\]" || resetprop -n -p "$PROP" ""
    done
fi

# Work around supported custom ROM PixelPropsUtils conflict when spoofProvider is disabled
if [ -n "$(resetprop persist.sys.pixelprops.pi)" ]; then
    resetprop -n -p persist.sys.pixelprops.pi false
    resetprop -n -p persist.sys.pixelprops.gapps false
    resetprop -n -p persist.sys.pixelprops.gms false
fi
