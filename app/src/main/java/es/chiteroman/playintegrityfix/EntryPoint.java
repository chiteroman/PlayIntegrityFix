package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.Log;

import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;

public class EntryPoint {
    public static void init() {
        spoofDevice();
        spoofProvider();
    }

    private static void spoofProvider() {
        try {
            Provider provider = Security.getProvider("AndroidKeyStore");
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");

            Field f = keyStore.getClass().getDeclaredField("keyStoreSpi");
            f.setAccessible(true);
            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) f.get(keyStore);
            f.setAccessible(false);

            CustomProvider customProvider = new CustomProvider(provider);
            Security.removeProvider("AndroidKeyStore");
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

    public static void spoofDevice() {
        setProp("PRODUCT", "WW_Phone");
        setProp("PRODUCT_FOR_ATTESTATION", "WW_Phone");

        setProp("DEVICE", "ASUS_X00HD_4");
        setProp("DEVICE_FOR_ATTESTATION", "ASUS_X00HD_4");

        setProp("MANUFACTURER", "Asus");
        setProp("MANUFACTURER_FOR_ATTESTATION", "Asus");

        setProp("BRAND", "Asus");
        setProp("BRAND_FOR_ATTESTATION", "Asus");

        setProp("MODEL", "ASUS_X00HD");
        setProp("MODEL_FOR_ATTESTATION", "ASUS_X00HD");

        setProp("FINGERPRINT", "asus/WW_Phone/ASUS_X00HD_4:7.1.1/NMF26F/14.2016.1801.372-20180119:user/release-keys");
    }

    private static void setProp(String name, String value) {
        try {
            Field f = Build.class.getDeclaredField(name);
            f.setAccessible(true);
            f.set(null, value);
            f.setAccessible(false);
            LOG(String.format("Modified field '%s' with value '%s'", name, value));
        } catch (NoSuchFieldException e) {
            LOG(String.format("Couldn't find '%s' field name.", name));
        } catch (IllegalAccessException e) {
            LOG(String.format("Couldn't modify '%s' field value.", name));
        }
    }

    public static void LOG(String msg) {
        Log.d("PIF/Java", msg);
    }
}