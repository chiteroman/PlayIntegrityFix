MODPATH="${0%/*}"
. $MODPATH/common_func.sh

# Remove Play Services from Magisk DenyList when set to Enforce in normal mode
if magisk --denylist status; then
    magisk --denylist rm com.google.android.gms
fi

# Conditional early sensitive properties

# Samsung
resetprop_if_diff ro.boot.warranty_bit 0
resetprop_if_diff ro.vendor.boot.warranty_bit 0
resetprop_if_diff ro.vendor.warranty_bit 0
resetprop_if_diff ro.warranty_bit 0

# Xiaomi
resetprop_if_diff ro.secureboot.lockstate locked

# Realme
resetprop_if_diff ro.boot.realmebootstate green

# OnePlus
resetprop_if_diff ro.is_ever_orange 0

# Microsoft
for PROP in $(resetprop | grep -oE 'ro.*.build.tags'); do
    resetprop_if_diff $PROP release-keys
done

# Other
for PROP in $(resetprop | grep -oE 'ro.*.build.type'); do
    resetprop_if_diff $PROP user
done
resetprop_if_diff ro.debuggable 0
resetprop_if_diff ro.force.debuggable 0
resetprop_if_diff ro.secure 1
