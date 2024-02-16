# Remove Play Services from Magisk Denylist when set to enforcing
if magisk --denylist status; then
    magisk --denylist rm com.google.android.gms
fi

# Remove safetynet-fix module if installed
if [ -d /data/adb/modules/safetynet-fix ]; then
    rm -rf /data/adb/modules/safetynet-fix
	rm -rf /data/adb/SNFix.dex
fi

resetprop_if_diff() {
    local NAME="$1"
    local EXPECTED="$2"
    local CURRENT="$(resetprop "$NAME")"

    [ -z "$CURRENT" ] || [ "$CURRENT" = "$EXPECTED" ] || resetprop "$NAME" "$EXPECTED"
}

resetprop_if_diff ro.build.tags release-keys

resetprop_if_diff ro.boot.warranty_bit 0

resetprop_if_diff ro.vendor.boot.warranty_bit 0

resetprop_if_diff ro.vendor.warranty_bit 0

resetprop_if_diff ro.warranty_bit 0

resetprop_if_diff ro.is_ever_orange 0

resetprop_if_diff ro.build.type user

resetprop_if_diff ro.debuggable 0

resetprop_if_diff ro.secure 1
