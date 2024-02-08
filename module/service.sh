# Conditional sensitive properties

resetprop_if_diff() {
    local NAME="$1"
    local EXPECTED="$2"
    local CURRENT="$(resetprop "$NAME")"

    [ -z "$CURRENT" ] || [ "$CURRENT" = "$EXPECTED" ] || resetprop "$NAME" "$EXPECTED"
}

resetprop_if_match() {
    local NAME="$1"
    local CONTAINS="$2"
    local VALUE="$3"

    [[ "$(resetprop "$NAME")" = *"$CONTAINS"* ]] && resetprop "$NAME" "$VALUE"
}

# Magisk recovery mode
resetprop_if_match ro.bootmode recovery unknown
resetprop_if_match ro.boot.mode recovery unknown
resetprop_if_match vendor.boot.mode recovery unknown

# SELinux
if [ -n "$(resetprop ro.build.selinux)" ]; then
    resetprop --delete ro.build.selinux
fi

# use toybox to protect *stat* access time reading
if [ "$(toybox cat /sys/fs/selinux/enforce)" == "0" ]; then
    chmod 640 /sys/fs/selinux/enforce
    chmod 440 /sys/fs/selinux/policy
fi

# Conditional late sensitive properties

# SafetyNet/Play Integrity
{
    # must be set after boot_completed for various OEMs
    until [ "$(getprop sys.boot_completed)" = "1" ]; do
        sleep 1
    done

    # avoid breaking Realme fingerprint scanners
    resetprop_if_diff ro.boot.flash.locked 1
    resetprop_if_diff ro.boot.realme.lockstate 1

    # avoid breaking Oppo fingerprint scanners
    resetprop_if_diff ro.boot.vbmeta.device_state locked

    # avoid breaking OnePlus display modes/fingerprint scanners
    resetprop_if_diff vendor.boot.verifiedbootstate green

    # avoid breaking OnePlus/Oppo display fingerprint scanners on OOS/ColorOS 12+
    resetprop_if_diff ro.boot.verifiedbootstate green
    resetprop_if_diff ro.boot.veritymode enforcing
    resetprop_if_diff vendor.boot.vbmeta.device_state locked
	
    # Xiaomi
    resetprop_if_diff ro.secureboot.lockstate locked

    # Realme
    resetprop_if_diff ro.boot.realmebootstate green
}&
