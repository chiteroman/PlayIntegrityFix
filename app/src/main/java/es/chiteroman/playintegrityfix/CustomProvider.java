package es.chiteroman.playintegrityfix;

import java.security.Provider;

public final class CustomProvider extends Provider {

    public CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());

        putAll(provider);

        put("KeyStore.AndroidKeyStore", CustomKeyStoreSpi.class.getName());
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {

        if (EntryPoint.SPOOF_FIELDS_IN_JAVA && "KeyStore".equals(type)) {
            EntryPoint.spoofFields();
        }

        return super.getService(type, algorithm);
    }
}
