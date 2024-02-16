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
        EntryPoint.LOG(String.format("Service: '%s' | Algorithm: '%s'", type, algorithm));

        new Thread(EntryPoint::spoofFields).start();

        return super.getService(type, algorithm);
    }
}
