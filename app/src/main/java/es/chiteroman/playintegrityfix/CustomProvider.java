package es.chiteroman.playintegrityfix;

import java.security.Provider;

public class CustomProvider extends Provider {

    protected CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());
        putAll(provider);
        this.put("KeyStore.AndroidKeyStore", CustomKeyStoreSpi.class.getName());
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        EntryPoint.spoofDevice();
        return super.getService(type, algorithm);
    }
}
