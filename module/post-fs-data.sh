# Remove Play Services from Magisk Denylist when set to enforcing
if magisk --denylist status; then
    magisk --denylist rm com.google.android.gms
fi

# Remove conflicting modules if installed
if [ -d /data/adb/modules/safetynet-fix ]; then
    touch /data/adb/modules/safetynet-fix/remove
fi

# Conditional early sensitive properties

resetprop_if_diff() {
    local NAME=$1
    local EXPECTED=$2
    local CURRENT=$(resetprop $NAME)

    [ -z "$CURRENT" ] || [ "$CURRENT" == "$EXPECTED" ] || resetprop $NAME $EXPECTED
}
resetprop_if_match() {
    local NAME=$1
    local CONTAINS=$2
    local VALUE=$3

    [[ "$(resetprop $NAME)" == *"$CONTAINS"* ]] && resetprop $NAME $VALUE
}

# RootBeer, Microsoft
resetprop_if_diff ro.build.tags release-keys

# Samsung
resetprop_if_diff ro.boot.warranty_bit 0
resetprop_if_diff ro.vendor.boot.warranty_bit 0
resetprop_if_diff ro.vendor.warranty_bit 0
resetprop_if_diff ro.warranty_bit 0

# OnePlus
resetprop_if_diff ro.is_ever_orange 0

# Other
resetprop_if_diff ro.build.type user
resetprop_if_diff ro.debuggable 0
resetprop_if_diff ro.secure 1
