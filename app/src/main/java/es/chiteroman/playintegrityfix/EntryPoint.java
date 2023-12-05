package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.JsonReader;
import android.util.Log;

import java.io.IOException;
import java.io.StringReader;
import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;

public final class EntryPoint {
    private static final Map<String, String> map = new HashMap<>();

    public static void init(String data) {
        try (JsonReader reader = new JsonReader(new StringReader(data))) {
            reader.beginObject();
            while (reader.hasNext()) {
                map.put(reader.nextName(), reader.nextString());
            }
            reader.endObject();
        } catch (IOException e) {
            LOG("Couldn't read JSON from Zygisk: " + e);
            return;
        }
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
        setProp("PRODUCT", map.get("PRODUCT"));
        setProp("DEVICE", map.get("DEVICE"));
        setProp("MANUFACTURER", map.get("MANUFACTURER"));
        setProp("BRAND", map.get("BRAND"));
        setProp("MODEL", map.get("MODEL"));
        setProp("FINGERPRINT", map.get("FINGERPRINT"));
        setProp("ID", map.get("BUILD_ID"));
        setVersionProp("SECURITY_PATCH", map.get("SECURITY_PATCH"));
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