package es.chiteroman.playintegrityfix;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.Key;
import java.security.KeyStoreException;
import java.security.KeyStoreSpi;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.util.Date;
import java.util.Enumeration;
import java.util.Locale;

public final class CustomKeyStoreSpi extends KeyStoreSpi {
    public static volatile KeyStoreSpi keyStoreSpi;

    @Override
    public Key engineGetKey(String alias, char[] password) throws NoSuchAlgorithmException, UnrecoverableKeyException {
        return keyStoreSpi.engineGetKey(alias, password);
    }

    @Override
    public Certificate[] engineGetCertificateChain(String alias) {
        for (StackTraceElement e : Thread.currentThread().getStackTrace()) {
            if (e.getClassName().toLowerCase(Locale.ROOT).contains("droidguard")) {
                EntryPoint.LOG("DroidGuard detected!");
                throw new UnsupportedOperationException();
            }
        }
        return keyStoreSpi.engineGetCertificateChain(alias);
    }

    @Override
    public Certificate engineGetCertificate(String alias) {
        return keyStoreSpi.engineGetCertificate(alias);
    }

    @Override
    public Date engineGetCreationDate(String alias) {
        return keyStoreSpi.engineGetCreationDate(alias);
    }

    @Override
    public void engineSetKeyEntry(String alias, Key key, char[] password, Certificate[] chain) throws KeyStoreException {
        keyStoreSpi.engineSetKeyEntry(alias, key, password, chain);
    }

    @Override
    public void engineSetKeyEntry(String alias, byte[] key, Certificate[] chain) throws KeyStoreException {
        keyStoreSpi.engineSetKeyEntry(alias, key, chain);
    }

    @Override
    public void engineSetCertificateEntry(String alias, Certificate cert) throws KeyStoreException {
        keyStoreSpi.engineSetCertificateEntry(alias, cert);
    }

    @Override
    public void engineDeleteEntry(String alias) throws KeyStoreException {
        keyStoreSpi.engineDeleteEntry(alias);
    }

    @Override
    public Enumeration<String> engineAliases() {
        return keyStoreSpi.engineAliases();
    }

    @Override
    public boolean engineContainsAlias(String alias) {
        return keyStoreSpi.engineContainsAlias(alias);
    }

    @Override
    public int engineSize() {
        return keyStoreSpi.engineSize();
    }

    @Override
    public boolean engineIsKeyEntry(String alias) {
        return keyStoreSpi.engineIsKeyEntry(alias);
    }

    @Override
    public boolean engineIsCertificateEntry(String alias) {
        return keyStoreSpi.engineIsCertificateEntry(alias);
    }

    @Override
    public String engineGetCertificateAlias(Certificate cert) {
        return keyStoreSpi.engineGetCertificateAlias(cert);
    }

    @Override
    public void engineStore(OutputStream stream, char[] password) throws CertificateException, IOException, NoSuchAlgorithmException {
        keyStoreSpi.engineStore(stream, password);
    }

    @Override
    public void engineLoad(InputStream stream, char[] password) throws CertificateException, IOException, NoSuchAlgorithmException {
        keyStoreSpi.engineLoad(stream, password);
    }
}
