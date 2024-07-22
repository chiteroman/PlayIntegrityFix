# Remove Play Services from Magisk DenyList when set to Enforce in normal mode
if magisk --denylist status; then
    magisk --denylist rm com.google.android.gms
fi

check_reset_prop() {
  local NAME=$1
  local EXPECTED=$2
  local VALUE=$(resetprop $NAME)
  [ -z $VALUE ] || [ $VALUE = $EXPECTED ] || resetprop $NAME $EXPECTED
}

# Conditional early sensitive properties

# Samsung
check_reset_prop ro.boot.warranty_bit 0
check_reset_prop ro.vendor.boot.warranty_bit 0
check_reset_prop ro.vendor.warranty_bit 0
check_reset_prop ro.warranty_bit 0

# Xiaomi
check_reset_prop ro.secureboot.lockstate locked

# Realme
check_reset_prop ro.boot.realmebootstate green

# OnePlus
check_reset_prop ro.is_ever_orange 0

# Microsoft
for PROP in $(resetprop | grep -oE 'ro.*.build.tags'); do
    check_reset_prop $PROP release-keys
done

# Other
for PROP in $(resetprop | grep -oE 'ro.*.build.type'); do
    check_reset_prop $PROP user
done
check_reset_prop ro.debuggable 0
check_reset_prop ro.force.debuggable 0
check_reset_prop ro.secure 1
