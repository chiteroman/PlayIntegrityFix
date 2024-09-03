# Sensitive properties

resetprop_if_diff() {
    local NAME="$1"
    local EXPECTED="$2"
    local CURRENT="$(resetprop "$NAME")"
    
    [ -z "$CURRENT" ] || [ "$CURRENT" = "$EXPECTED" ] || resetprop -n "$NAME" "$EXPECTED"
}

resetprop_if_match() {
    local NAME="$1"
    local CONTAINS="$2"
    local VALUE="$3"
    
    [[ "$(resetprop "$NAME")" = *"$CONTAINS"* ]] && resetprop -n "$NAME" "$VALUE"
}

# Magisk recovery mode
resetprop_if_match ro.bootmode recovery unknown
resetprop_if_match ro.boot.mode recovery unknown
resetprop_if_match vendor.boot.mode recovery unknown

# Hiding SELinux | Permissive status
resetprop_if_diff ro.boot.selinux enforcing
if [ -n "$(resetprop ro.build.selinux)" ]; then
    resetprop --delete ro.build.selinux
fi

# Hiding SELinux | Use toybox to protect *stat* access time reading
if [[ "$(toybox cat /sys/fs/selinux/enforce)" == "0" ]]; then
    chmod 640 /sys/fs/selinux/enforce
    chmod 440 /sys/fs/selinux/policy
fi

# Late props which must be set after boot_completed
{
    until [[ "$(getprop sys.boot_completed)" == "1" ]]; do
        sleep 1
    done
    
    # SafetyNet/Play Integrity | Avoid breaking Realme fingerprint scanners
    resetprop_if_diff ro.boot.flash.locked 1
    
    # SafetyNet/Play Integrity | Avoid breaking Oppo fingerprint scanners
    resetprop_if_diff ro.boot.vbmeta.device_state locked
    
    # SafetyNet/Play Integrity | Avoid breaking OnePlus display modes/fingerprint scanners
    resetprop_if_diff vendor.boot.verifiedbootstate green
    
    # SafetyNet/Play Integrity | Avoid breaking OnePlus display modes/fingerprint scanners on OOS 12
    resetprop_if_diff ro.boot.verifiedbootstate green
    resetprop_if_diff ro.boot.veritymode enforcing
    resetprop_if_diff vendor.boot.vbmeta.device_state locked

    # Custom ROMs support
    resetprop_if_diff persist.sys.pixelprops.pi false
}&
