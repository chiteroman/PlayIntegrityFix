package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.JsonReader;
import android.util.Log;

import java.io.FileReader;
import java.lang.reflect.Field;
import java.security.KeyStore;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Properties;

public class EntryPoint {
    private static final Map<String, String> map = new HashMap<>();

    public static void init(String file) {

        if (file.endsWith(".json")) {

            try (JsonReader reader = new JsonReader(new FileReader(file))) {
                reader.beginObject();
                while (reader.hasNext()) {
                    String key = reader.nextName();
                    String value = reader.nextString();
                    map.put(key, value);
                }
                reader.endObject();
            } catch (Exception e) {
                LOG("Error parsing JSON file: " + e);
            }

        } else if (file.endsWith(".prop")) {

            try {
                Properties properties = new Properties();

                properties.load(new FileReader(file));

                properties.forEach((o, o2) -> map.put((String) o, (String) o2));

            } catch (Exception e) {
                LOG("Error parsing PROP file: " + e);
            }
        }

        LOG("Map info (keys and values):");
        map.forEach((s, s2) -> LOG(String.format("[%s] -> %s", s, s2)));

        spoofDevice();
        spoofProvider();
    }

    protected static void LOG(String msg) {
        Log.d("PIF/Java", msg);
    }

    protected static void spoofDevice() {
        map.forEach(EntryPoint::setFieldValue);
    }

    protected static boolean isDroidGuard() {
        return Arrays.stream(Thread.currentThread().getStackTrace()).anyMatch(e -> e.getClassName().toLowerCase(Locale.US).contains("droidguard"));
    }

    private static void spoofProvider() {
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");

            Field field = keyStore.getClass().getDeclaredField("keyStoreSpi");

            field.setAccessible(true);
            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) field.get(keyStore);

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
            if (value.equals(oldValue)) return;
            field.set(null, value);
        } catch (IllegalAccessException e) {
            LOG("Couldn't get or set field: " + e);
        }
        LOG(String.format("Field '%s' with value '%s' is now set to '%s'", name, oldValue, value));
    }
}