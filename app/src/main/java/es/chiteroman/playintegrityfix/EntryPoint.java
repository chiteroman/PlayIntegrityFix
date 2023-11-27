package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.Log;

import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;

public final class EntryPoint {
    private static final String PRODUCT = "sailfish";
    private static final String DEVICE = "sailfish";
    private static final String MANUFACTURER = "Google";
    private static final String BRAND = "google";
    private static final String MODEL = "Pixel";
    private static final String FINGERPRINT = "google/sailfish/sailfish:8.1.0/OPM4.171019.021.P1/4820305:user/release-keys";
    private static final String SECURITY_PATCH = "2018-07-05";

    public static void init() {
        spoofProvider();
        spoofDevice();
    }

    private static void spoofProvider() {
        final String KEYSTORE = "AndroidKeyStore";

        try {
            Provider provider = Security.getProvider(KEYSTORE);
            KeyStore keyStore = KeyStore.getInstance(KEYSTORE);

            Field f = keyStore.getClass().getDeclaredField("keyStoreSpi");
            f.setAccessible(true);
            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) f.get(keyStore);
            f.setAccessible(false);

            CustomProvider customProvider = new CustomProvider(provider);
            Security.removeProvider(KEYSTORE);
            Security.insertProviderAt(customProvider, 1);

            LOG("Spoof KeyStoreSpi and Provider done!");

        } catch (KeyStoreException e) {
            LOG("Couldn't find KeyStore: " + e);
        } catch (NoSuchFieldException e) {
            LOG("Couldn't find field: " + e);
        } catch (IllegalAccessException e) {
            LOG("Couldn't change access of field: " + e);
        }
    }

    static void spoofDevice() {
        setProp("PRODUCT", PRODUCT);
        setProp("DEVICE", DEVICE);
        setProp("MANUFACTURER", MANUFACTURER);
        setProp("BRAND", BRAND);
        setProp("MODEL", MODEL);
        setProp("FINGERPRINT", FINGERPRINT);
        setVersionProp("SECURITY_PATCH", SECURITY_PATCH);
    }

    private static void setProp(String name, String value) {
        if (name == null || value == null || name.isEmpty() || value.isEmpty()) return;
        try {
            Field field = Build.class.getDeclaredField(name);
            field.setAccessible(true);
            String oldValue = (String) field.get(null);
            field.set(null, value);
            field.setAccessible(false);
            if (value.equals(oldValue)) return;
            LOG(String.format("[%s]: %s -> %s", name, oldValue, value));
        } catch (NoSuchFieldException e) {
            LOG(String.format("Couldn't find '%s' field name.", name));
        } catch (IllegalAccessException e) {
            LOG(String.format("Couldn't modify '%s' field value.", name));
        }
    }

    private static void setVersionProp(String name, String value) {
        if (name == null || value == null || name.isEmpty() || value.isEmpty()) return;
        try {
            Field field = Build.VERSION.class.getDeclaredField(name);
            field.setAccessible(true);
            String oldValue = (String) field.get(null);
            field.set(null, value);
            field.setAccessible(false);
            if (value.equals(oldValue)) return;
            LOG(String.format("[%s]: %s -> %s", name, oldValue, value));
        } catch (NoSuchFieldException e) {
            LOG(String.format("Couldn't find '%s' field name.", name));
        } catch (IllegalAccessException e) {
            LOG(String.format("Couldn't modify '%s' field value.", name));
        }
    }

    static void LOG(String msg) {
        Log.d("PIF/Java", msg);
    }
}