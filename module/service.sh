# Sensitive properties

maybe_set_prop() {
    local prop="$1"
    local contains="$2"
    local value="$3"

    if [[ "$(getprop "$prop")" == *"$contains"* ]]; then
        resetprop "$prop" "$value"
    fi
}

# Magisk recovery mode
maybe_set_prop ro.bootmode recovery unknown
maybe_set_prop ro.boot.mode recovery unknown
maybe_set_prop vendor.boot.mode recovery unknown

# Hiding SELinux | Permissive status
resetprop --delete ro.build.selinux

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
    resetprop ro.boot.flash.locked 1

    # SafetyNet/Play Integrity | Avoid breaking Oppo fingerprint scanners
    resetprop ro.boot.vbmeta.device_state locked

    # SafetyNet/Play Integrity | Avoid breaking OnePlus display modes/fingerprint scanners
    resetprop vendor.boot.verifiedbootstate green

    # SafetyNet/Play Integrity | Avoid breaking OnePlus display modes/fingerprint scanners on OOS 12
    resetprop ro.boot.verifiedbootstate green
    resetprop ro.boot.veritymode enforcing
    resetprop vendor.boot.vbmeta.device_state locked
}&
