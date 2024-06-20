package es.chiteroman.playintegrityfix;

import android.security.keystore.KeyProperties;
import android.text.TextUtils;

import org.spongycastle.asn1.ASN1Boolean;
import org.spongycastle.asn1.ASN1Encodable;
import org.spongycastle.asn1.ASN1EncodableVector;
import org.spongycastle.asn1.ASN1Enumerated;
import org.spongycastle.asn1.ASN1ObjectIdentifier;
import org.spongycastle.asn1.ASN1OctetString;
import org.spongycastle.asn1.ASN1Sequence;
import org.spongycastle.asn1.ASN1TaggedObject;
import org.spongycastle.asn1.DEROctetString;
import org.spongycastle.asn1.DERSequence;
import org.spongycastle.asn1.DERTaggedObject;
import org.spongycastle.asn1.x509.Extension;
import org.spongycastle.cert.X509CertificateHolder;
import org.spongycastle.cert.X509v3CertificateBuilder;
import org.spongycastle.cert.jcajce.JcaX509CertificateConverter;
import org.spongycastle.openssl.PEMKeyPair;
import org.spongycastle.openssl.PEMParser;
import org.spongycastle.openssl.jcajce.JcaPEMKeyConverter;
import org.spongycastle.operator.ContentSigner;
import org.spongycastle.operator.jcajce.JcaContentSignerBuilder;
import org.spongycastle.util.io.pem.PemReader;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import java.io.ByteArrayInputStream;
import java.io.StringReader;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.LinkedList;
import java.util.concurrent.ThreadLocalRandom;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

public final class KeyboxUtils {
    private static final ASN1ObjectIdentifier OID = new ASN1ObjectIdentifier("1.3.6.1.4.1.11129.2.1.17");
    private static final LinkedList<Certificate> EC_CERTS = new LinkedList<>();
    private static final LinkedList<Certificate> RSA_CERTS = new LinkedList<>();
    private static final CertificateFactory certificateFactory;
    private static PEMKeyPair EC, RSA;

    static {
        try {
            certificateFactory = CertificateFactory.getInstance("X.509");
        } catch (CertificateException e) {
            throw new RuntimeException(e);
        }
    }

    public static void parseXml(String kbox) throws Throwable {
        if (TextUtils.isEmpty(kbox)) return;

        DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
        DocumentBuilder builder = factory.newDocumentBuilder();
        Document doc = builder.parse(new ByteArrayInputStream(kbox.getBytes()));

        doc.getDocumentElement().normalize();

        NodeList keyboxList = doc.getElementsByTagName("Keybox");
        Node keyboxNode = keyboxList.item(0);
        if (keyboxNode.getNodeType() == Node.ELEMENT_NODE) {
            Element keyboxElement = (Element) keyboxNode;

            NodeList keyList = keyboxElement.getElementsByTagName("Key");
            for (int j = 0; j < keyList.getLength(); j++) {
                Element keyElement = (Element) keyList.item(j);
                String algorithm = keyElement.getAttribute("algorithm");

                NodeList privateKeyList = keyElement.getElementsByTagName("PrivateKey");
                if (privateKeyList.getLength() > 0) {
                    Element privateKeyElement = (Element) privateKeyList.item(0);
                    String privateKeyContent = privateKeyElement.getTextContent().trim();
                    if ("ecdsa".equals(algorithm)) {
                        EC = parseKeyPair(privateKeyContent);
                    } else if ("rsa".equals(algorithm)) {
                        RSA = parseKeyPair(privateKeyContent);
                    }
                }

                NodeList certificateChainList = keyElement.getElementsByTagName("CertificateChain");
                if (certificateChainList.getLength() > 0) {
                    Element certificateChainElement = (Element) certificateChainList.item(0);

                    NodeList certificateList = certificateChainElement.getElementsByTagName("Certificate");
                    for (int k = 0; k < certificateList.getLength(); k++) {
                        Element certificateElement = (Element) certificateList.item(k);
                        String certificateContent = certificateElement.getTextContent().trim();
                        if ("ecdsa".equals(algorithm)) {
                            EC_CERTS.add(parseCert(certificateContent));
                        } else if ("rsa".equals(algorithm)) {
                            RSA_CERTS.add(parseCert(certificateContent));
                        }
                    }
                }
            }
        }
    }

    private static PEMKeyPair parseKeyPair(String key) throws Throwable {
        try (PEMParser parser = new PEMParser(new StringReader(key))) {
            return (PEMKeyPair) parser.readObject();
        }
    }

    private static Certificate parseCert(String cert) throws Throwable {
        try (PemReader reader = new PemReader(new StringReader(cert))) {
            return certificateFactory.generateCertificate(new ByteArrayInputStream(reader.readPemObject().getContent()));
        }
    }

    public static Certificate[] engineGetCertificateChain(Certificate[] caList) {
        if (caList == null) {
            EntryPoint.LOG("Certificate chain is null!");
            throw new UnsupportedOperationException();
        }
        if (EC == null && RSA == null) {
            EntryPoint.LOG("EC and RSA private keys are null!");
            throw new UnsupportedOperationException();
        }
        if (EC_CERTS.isEmpty() && RSA_CERTS.isEmpty()) {
            EntryPoint.LOG("EC and RSA certs are empty!");
            throw new UnsupportedOperationException();
        }
        try {
            X509Certificate leaf = (X509Certificate) certificateFactory.generateCertificate(new ByteArrayInputStream(caList[0].getEncoded()));

            byte[] bytes = leaf.getExtensionValue(OID.getId());

            if (bytes == null) return caList;

            X509CertificateHolder holder = new X509CertificateHolder(leaf.getEncoded());

            Extension ext = holder.getExtension(OID);

            ASN1Sequence sequence = ASN1Sequence.getInstance(ext.getExtnValue().getOctets());

            ASN1Encodable[] encodables = sequence.toArray();

            ASN1Sequence teeEnforced = (ASN1Sequence) encodables[7];

            ASN1EncodableVector vector = new ASN1EncodableVector();

            for (ASN1Encodable asn1Encodable : teeEnforced) {
                ASN1TaggedObject taggedObject = (ASN1TaggedObject) asn1Encodable;
                if (taggedObject.getTagNo() == 704) continue;
                vector.add(taggedObject);
            }

            LinkedList<Certificate> certificates;

            X509v3CertificateBuilder builder;
            ContentSigner signer;

            // Not all keyboxes have EC keys :)
            if (EC != null && !EC_CERTS.isEmpty() && KeyProperties.KEY_ALGORITHM_EC.equals(leaf.getPublicKey().getAlgorithm())) {
                EntryPoint.LOG("Using EC");
                certificates = new LinkedList<>(EC_CERTS);
                builder = new X509v3CertificateBuilder(new X509CertificateHolder(EC_CERTS.get(0).getEncoded()).getSubject(), holder.getSerialNumber(), holder.getNotBefore(), holder.getNotAfter(), holder.getSubject(), EC.getPublicKeyInfo());
                signer = new JcaContentSignerBuilder(leaf.getSigAlgName()).build(new JcaPEMKeyConverter().getPrivateKey(EC.getPrivateKeyInfo()));
            } else {
                EntryPoint.LOG("Using RSA");
                certificates = new LinkedList<>(RSA_CERTS);
                builder = new X509v3CertificateBuilder(new X509CertificateHolder(RSA_CERTS.get(0).getEncoded()).getSubject(), holder.getSerialNumber(), holder.getNotBefore(), holder.getNotAfter(), holder.getSubject(), RSA.getPublicKeyInfo());
                signer = new JcaContentSignerBuilder("SHA256withRSA").build(new JcaPEMKeyConverter().getPrivateKey(RSA.getPrivateKeyInfo()));
            }

            byte[] verifiedBootKey = new byte[32];
            byte[] verifiedBootHash = new byte[32];

            ThreadLocalRandom.current().nextBytes(verifiedBootKey);
            ThreadLocalRandom.current().nextBytes(verifiedBootHash);

            ASN1Encodable[] rootOfTrustEnc = {new DEROctetString(verifiedBootKey), ASN1Boolean.TRUE, new ASN1Enumerated(0), new DEROctetString(verifiedBootHash)};

            ASN1Sequence rootOfTrustSeq = new DERSequence(rootOfTrustEnc);

            ASN1TaggedObject rootOfTrustTagObj = new DERTaggedObject(704, rootOfTrustSeq);

            vector.add(rootOfTrustTagObj);

            ASN1Sequence hackEnforced = new DERSequence(vector);

            encodables[7] = hackEnforced;

            ASN1Sequence hackedSeq = new DERSequence(encodables);

            ASN1OctetString hackedSeqOctets = new DEROctetString(hackedSeq);

            Extension hackedExt = new Extension(OID, false, hackedSeqOctets);

            builder.addExtension(hackedExt);

            for (ASN1ObjectIdentifier extensionOID : holder.getExtensions().getExtensionOIDs()) {
                if (OID.getId().equals(extensionOID.getId())) continue;
                builder.addExtension(holder.getExtension(extensionOID));
            }

            certificates.addFirst(new JcaX509CertificateConverter().getCertificate(builder.build(signer)));

            return certificates.toArray(new Certificate[0]);

        } catch (Throwable t) {
            EntryPoint.LOG(t.toString());
        }
        throw new UnsupportedOperationException();
    }
}