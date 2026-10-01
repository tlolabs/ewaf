use base64::{engine::general_purpose::STANDARD as B64, Engine};
use ring::signature::{Ed25519KeyPair, KeyPair};
use sha2::{Digest, Sha256};
use std::io::{Read, Write};
use tlo_updater::*;
const NOW: u64 = 1_800_000_000;
fn fixture() -> (Trust, Manifest) {
    let key = Ed25519KeyPair::from_seed_unchecked(&[17; 32]).unwrap();
    let trust = Trust {
        application_id: "com.tlolabs.test".into(),
        repository: "tlolabs/test".into(),
        keys: [("test".into(), B64.encode(key.public_key().as_ref()))].into(),
        macos_team_id: "TEAM".into(),
        windows_publisher: "CN=TLO".into(),
    };
    let filename = "test-2.10.0-linux-x64.AppImage";
    let manifest = Manifest {
        schema: 1,
        application_id: trust.application_id.clone(),
        repository: trust.repository.clone(),
        version: "2.10.0".into(),
        tag: "v2.10.0".into(),
        channel: "stable".into(),
        draft: false,
        published_at: "2027-01-15T08:00:00Z".into(),
        expires_at: NOW + 86400,
        release_notes_url: trust.release_url("2.10.0"),
        restart_required: true,
        migration: None,
        artifacts: vec![Artifact {
            platform: "linux".into(),
            architecture: "x64".into(),
            minimum_os: "24.04".into(),
            format: "AppImage".into(),
            filename: filename.into(),
            url: trust.asset_url("2.10.0", filename),
            size: 7,
            sha256: format!("{:x}", Sha256::digest(b"payload")),
            signer: "manifest-ed25519".into(),
            sparkle_signature: None,
        }],
    };
    (trust, manifest)
}
fn sign(m: &Manifest) -> Vec<u8> {
    let key = Ed25519KeyPair::from_seed_unchecked(&[17; 32]).unwrap();
    let payload = serde_json::to_vec(m).unwrap();
    serde_json::to_vec(&Envelope {
        key_id: "test".into(),
        payload: B64.encode(&payload),
        signature: B64.encode(key.sign(&payload)),
    })
    .unwrap()
}
#[test]
fn valid_newer_numeric_semver() {
    let (t, m) = fixture();
    let r = verify(&sign(&m), &t, NOW).unwrap();
    assert!(r
        .candidate("2.9.0", "linux", "x64", "24.04")
        .unwrap()
        .is_some());
}
#[test]
fn same_older_and_no_matching_target() {
    let (t, m) = fixture();
    let r = verify(&sign(&m), &t, NOW).unwrap();
    for v in ["2.10.0", "3.0.0"] {
        assert!(r.candidate(v, "linux", "x64", "24.04").unwrap().is_none());
    }
    for (p, a, os) in [
        ("windows", "x64", "24.04"),
        ("linux", "arm64", "24.04"),
        ("linux", "x64", "22.04"),
    ] {
        assert!(r.candidate("1.0.0", p, a, os).unwrap().is_none());
    }
}
#[test]
fn reject_malformed_stable_versions() {
    for v in [
        "1.2",
        "01.2.3",
        "v1.2.3",
        "1.2.3-rc.1",
        "1.2.3+build",
        "nonsense",
    ] {
        assert!(stable_version(v).is_err(), "{v}");
    }
}
#[test]
fn authenticated_bad_metadata_is_rejected() {
    let (t, m) = fixture();
    let cases: Vec<fn(&mut Manifest)> = vec![
        |m| m.application_id = "other".into(),
        |m| m.repository = "other/repo".into(),
        |m| m.channel = "nightly".into(),
        |m| m.draft = true,
        |m| m.version = "2.11.0-beta.1".into(),
        |m| m.tag = "main".into(),
        |m| m.schema = 2,
        |m| m.expires_at = NOW,
        |m| m.release_notes_url = "https://evil.invalid".into(),
        |m| m.artifacts[0].platform = "other".into(),
        |m| m.artifacts[0].architecture = "x86".into(),
        |m| m.artifacts[0].url.push_str("?redirect=evil"),
        |m| m.artifacts[0].url = m.artifacts[0].url.replace("v2.10.0", "v2.9.0"),
        |m| m.artifacts[0].filename = "../other.AppImage".into(),
        |m| m.artifacts[0].size = 0,
        |m| m.artifacts[0].sha256 = "0".repeat(63),
        |m| m.artifacts[0].signer = "OTHER".into(),
        |m| m.artifacts.push(m.artifacts[0].clone()),
        |m| m.artifacts[0].minimum_os = "garbage".into(),
    ];
    for (i, change) in cases.into_iter().enumerate() {
        let mut candidate = m.clone();
        change(&mut candidate);
        assert!(verify(&sign(&candidate), &t, NOW).is_err(), "case {i}");
    }
}
#[test]
fn reject_signature_substitution_and_unknown_keys() {
    let (t, m) = fixture();
    let bytes = sign(&m);
    let mut e: Envelope = serde_json::from_slice(&bytes).unwrap();
    e.payload = B64.encode(b"{}");
    assert!(verify(&serde_json::to_vec(&e).unwrap(), &t, NOW).is_err());
    let mut e: Envelope = serde_json::from_slice(&bytes).unwrap();
    e.signature = B64.encode([0; 64]);
    assert!(verify(&serde_json::to_vec(&e).unwrap(), &t, NOW).is_err());
    e.key_id = "attacker".into();
    assert!(verify(&serde_json::to_vec(&e).unwrap(), &t, NOW).is_err());
    let mut t = t;
    t.keys.clear();
    assert!(verify(&bytes, &t, NOW).is_err());
}
#[test]
fn bounded_and_malformed_manifest() {
    let (t, _) = fixture();
    assert!(read_release(&vec![0; MAX_MANIFEST as usize + 1][..], &t, NOW).is_err());
    assert!(verify(b"{}", &t, NOW).is_err());
}
#[test]
fn digest_size_and_interrupted_download() {
    let (_, m) = fixture();
    let a = &m.artifacts[0];
    let mut output = Vec::new();
    copy_verified(&b"payload"[..], &mut output, a).unwrap();
    assert_eq!(output, b"payload");
    for bad in [&b"payloa"[..], &b"payload!"[..], &b"corrupt"[..]] {
        assert!(copy_verified(bad, std::io::sink(), a).is_err());
    }
    struct Interrupted;
    impl Read for Interrupted {
        fn read(&mut self, _: &mut [u8]) -> std::io::Result<usize> {
            Err(std::io::Error::new(
                std::io::ErrorKind::ConnectionReset,
                "interrupted",
            ))
        }
    }
    assert!(copy_verified(Interrupted, std::io::sink(), a).is_err());
    let mut wrong = a.clone();
    wrong.sha256 = "0".repeat(64);
    assert!(copy_verified(&b"payload"[..], std::io::sink(), &wrong).is_err());
}
#[test]
fn automatic_policy_and_failure_backoff() {
    assert!(should_check(NOW, 0, 0, true, false));
    assert!(!should_check(NOW, NOW - 1, 0, true, false));
    assert!(!should_check(NOW, 0, NOW - 1, true, false));
    assert!(!should_check(NOW, 0, 0, false, false));
    assert!(should_check(NOW, NOW, NOW, false, true));
    assert!(should_check(NOW, NOW + 1, NOW + 1, true, false));
}
#[test]
fn mock_release_endpoint_discovery_authentication_download() {
    let (t, m) = fixture();
    let envelope = sign(&m);
    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    let server = std::thread::spawn(move || {
        for body in [envelope, b"payload".to_vec()] {
            let (mut stream, _) = listener.accept().unwrap();
            let mut req = [0; 4096];
            assert!(stream.read(&mut req).unwrap() > 0);
            write!(
                stream,
                "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
                body.len()
            )
            .unwrap();
            stream.write_all(&body).unwrap();
        }
    });
    // HTTP exists only inside this test. Production transport has no endpoint override.
    let client = reqwest::blocking::Client::new();
    let r = read_release(
        client
            .get(format!("http://{addr}/manifest"))
            .send()
            .unwrap(),
        &t,
        NOW,
    )
    .unwrap();
    let a = r
        .candidate("2.9.0", "linux", "x64", "24.04")
        .unwrap()
        .unwrap();
    copy_verified(
        client
            .get(format!("http://{addr}/artifact"))
            .send()
            .unwrap(),
        std::io::sink(),
        a,
    )
    .unwrap();
    server.join().unwrap();
}
#[test]
fn unavailable_service_and_https_boundary() {
    assert!(client().unwrap().get("http://127.0.0.1:1").send().is_err());
    let listener = std::net::TcpListener::bind("127.0.0.1:0").unwrap();
    let addr = listener.local_addr().unwrap();
    let server = std::thread::spawn(move || {
        let (mut s, _) = listener.accept().unwrap();
        let mut b = [0; 4096];
        assert!(s.read(&mut b).unwrap() > 0);
        s.write_all(
            b"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        )
        .unwrap();
    });
    assert!(reqwest::blocking::get(format!("http://{addr}"))
        .unwrap()
        .error_for_status()
        .is_err());
    server.join().unwrap();
}

#[cfg(target_os = "macos")]
#[test]
#[ignore = "requires the checksum-pinned Sparkle signing tool; run in macOS packaging CI"]
fn sparkle_and_shared_signer_interoperate() {
    use std::process::{Command, Stdio};
    let tool = std::env::var("SPARKLE_SIGN_TOOL")
        .expect("SPARKLE_SIGN_TOOL must name the pinned sign_update");
    let (_, manifest) = fixture();
    let directory = tempfile::tempdir().unwrap();
    let payload = directory.path().join("payload.json");
    std::fs::write(&payload, serde_json::to_vec(&manifest).unwrap()).unwrap();
    // Public deterministic test key, never a production credential.
    let seed = B64.encode([17; 32]);
    let mut command = Command::new(&tool)
        .args(["--ed-key-file", "-", "-p"])
        .arg(&payload)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    command
        .stdin
        .take()
        .unwrap()
        .write_all(seed.as_bytes())
        .unwrap();
    let output = command.wait_with_output().unwrap();
    assert!(output.status.success());
    let envelope: Envelope = serde_json::from_slice(&sign(&manifest)).unwrap();
    assert_eq!(
        String::from_utf8(output.stdout).unwrap().trim(),
        envelope.signature
    );
    let feed = directory.path().join("appcast.xml");
    std::fs::write(
        &feed,
        b"<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>Test</title></channel></rss>",
    )
    .unwrap();
    for verify in [false, true] {
        let mut builder = Command::new(&tool);
        builder.args(["--ed-key-file", "-"]);
        if verify {
            builder.arg("--verify");
        }
        let mut child = builder
            .arg(&feed)
            .stdin(Stdio::piped())
            .stdout(Stdio::null())
            .spawn()
            .unwrap();
        child
            .stdin
            .take()
            .unwrap()
            .write_all(seed.as_bytes())
            .unwrap();
        assert!(child.wait().unwrap().success());
    }
}

#[test]
fn special_migration_never_enters_automatic_install_path() {
    let (trust, mut manifest) = fixture();
    manifest.migration = Some("Follow the documented backup procedure".into());
    let release = verify(&sign(&manifest), &trust, NOW).unwrap();
    assert!(release.candidate("1.0.0", "linux", "x64", "24.04").is_err());
}
#[test]
fn downloader_rejects_altered_authenticated_artifact_fields_before_network() {
    let (trust, manifest) = fixture();
    let release = verify(&sign(&manifest), &trust, NOW).unwrap();
    let mut altered = manifest.artifacts[0].clone();
    altered.architecture = "arm64".into();
    let directory = tempfile::tempdir().unwrap();
    let error = download(&release, &altered, directory.path()).unwrap_err();
    assert!(error
        .to_string()
        .contains("not in the authenticated release"));
}
