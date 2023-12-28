package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;

public final class EntryPoint {
    private static JSONObject jsonObject = new JSONObject();

    public static void init(String json) {

        try {
            jsonObject = new JSONObject(json);
        } catch (JSONException e) {
            LOG("Couldn't parse JSON from Zygisk");
        }

        boolean FORCE_BASIC_ATTESTATION = true;

        if (jsonObject.has("FORCE_BASIC_ATTESTATION")) {
            try {
                FORCE_BASIC_ATTESTATION = jsonObject.getBoolean("FORCE_BASIC_ATTESTATION");
            } catch (JSONException e) {
                LOG("Couldn't parse FORCE_BASIC_ATTESTATION from JSON");
            }
            jsonObject.remove("FORCE_BASIC_ATTESTATION");
        }

        spoofDevice();

        if (FORCE_BASIC_ATTESTATION) spoofProvider();
    }

    static void LOG(String msg) {
        Log.d("PIF/Java", msg);
    }

    static void spoofDevice() {
        jsonObject.keys().forEachRemaining(s -> {
            try {
                Object value = jsonObject.get(s);
                setFieldValue(s, value);
            } catch (JSONException ignored) {
            }
        });
    }

    private static void spoofProvider() {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            keyStore.load(null);

            Field f = keyStore.getClass().getDeclaredField("keyStoreSpi");
            f.setAccessible(true);
            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) f.get(keyStore);
            f.setAccessible(false);

            Provider provider = Security.getProvider("AndroidKeyStore");

            Provider customProvider = new CustomProvider(provider);

            Security.removeProvider("AndroidKeyStore");
            Security.insertProviderAt(customProvider, 1);

            LOG("Spoof KeyStoreSpi and Provider done!");

        } catch (Throwable t) {
            LOG("spoofProvider exception: " + t);
        }
    }

    private static void setFieldValue(String name, Object value) {
        if (name == null || value == null || name.isEmpty()) return;

        if (value instanceof String str) if (str.isEmpty() || str.isBlank()) return;

        Field field = null;

        try {
            field = Build.class.getDeclaredField(name);
        } catch (NoSuchFieldException e) {
            try {
                field = Build.VERSION.class.getDeclaredField(name);
            } catch (NoSuchFieldException ex) {
                LOG("Couldn't find field: " + e);
            }
        }

        if (field == null) return;

        field.setAccessible(true);
        try {

            Object oldValue = field.get(null);

            if (!value.equals(oldValue)) {

                field.set(null, value);
                LOG("Set [" + name + "] field value to [" + value + "]");
            }

        } catch (IllegalAccessException e) {
            LOG("Couldn't modify field: " + e);
        }
        field.setAccessible(false);
    }
}