package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.JsonReader;
import android.util.Log;

import org.json.JSONException;

import java.io.StringReader;
import java.lang.reflect.Field;
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
                String key = reader.nextName();
                String value = reader.nextString();

                if (key == null || key.isEmpty() || key.isBlank() || value == null || value.isEmpty() || value.isBlank())
                    throw new JSONException("Empty key or value");

                map.put(key, value);
            }
            reader.endObject();
        } catch (Exception e) {
            LOG("Couldn't parse JSON from Zygisk lib: " + e);
            LOG("Remove /data/adb/pif.json");
        }

        spoofDevice();
        spoofProvider();
    }

    static void LOG(String msg) {
        Log.d("PIF/Java", msg);
    }

    static void spoofDevice() {
        map.forEach(EntryPoint::setFieldValue);
    }

    private static void spoofProvider() {
        try {
            Provider provider = Security.getProvider("AndroidKeyStore");

            Provider customProvider = new CustomProvider(provider);

            Security.removeProvider("AndroidKeyStore");
            Security.insertProviderAt(customProvider, 1);

            LOG("Spoof KeyStoreSpi and Provider done!");

        } catch (Exception e) {
            LOG("spoofProvider exception: " + e);
        }
    }

    private static void setFieldValue(String name, String value) {
        if (name == null || value == null || name.isEmpty() || value.isEmpty()) return;
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
        String oldValue = null;
        try {
            oldValue = (String) field.get(null);
            field.set(null, value);
        } catch (IllegalAccessException e) {
            LOG("Couldn't get or set field: " + e);
        }
        field.setAccessible(false);
        if (value.equals(oldValue)) return;
        LOG(String.format("[%s]: %s -> %s", name, oldValue, value));
    }
}