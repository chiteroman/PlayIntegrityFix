package es.chiteroman.playintegrityfix;

import java.security.Provider;
import java.security.ProviderException;

public class CustomProvider extends Provider {

    protected CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());

        putAll(provider);

        put("KeyStore.AndroidKeyStore", CustomKeyStoreSpi.class.getName());
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        EntryPoint.LOG("[SERVICE] Type: " + type + " | Algorithm: " + algorithm);

        EntryPoint.spoofDevice();

        if ("KeyPairGenerator".equals(type)) throw new ProviderException();

        return super.getService(type, algorithm);
    }
}
