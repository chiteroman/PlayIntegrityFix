package es.chiteroman.playintegrityfix;

import java.security.Provider;

public final class CustomProvider extends Provider {

    public CustomProvider(Provider provider, boolean spoof) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());

        putAll(provider);

        if (spoof) put("KeyStore.AndroidKeyStore", CustomKeyStoreSpi.class.getName());
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        EntryPoint.LOG(String.format("Service: '%s' | Algorithm: '%s'", type, algorithm));

        if ("AndroidKeyStore".equals(algorithm)) EntryPoint.spoofFields();

        return super.getService(type, algorithm);
    }
}
