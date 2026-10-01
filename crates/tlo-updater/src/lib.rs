// SPDX-License-Identifier: GPL-3.0-or-later
//! Standalone trust boundary; no application core or native UI dependencies.
use base64::{engine::general_purpose::STANDARD as B64, Engine};
use ring::signature::{UnparsedPublicKey, ED25519};
use semver::Version;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::{
    collections::BTreeMap,
    io::{Read, Write},
    path::Path,
    time::Duration,
};
pub type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;
pub const MAX_MANIFEST: u64 = 1024 * 1024;
pub const CHECK_INTERVAL: u64 = 24 * 60 * 60;
pub const RETRY_INTERVAL: u64 = 60 * 60;

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Trust {
    pub application_id: String,
    pub repository: String,
    pub keys: BTreeMap<String, String>,
    pub macos_team_id: String,
    pub windows_publisher: String,
    pub linux_gpg_fingerprint: String,
}
impl Trust {
    pub fn validate(&self) -> Result<()> {
        let parts: Vec<_> = self.repository.split('/').collect();
        if parts.len() != 2
            || parts.iter().any(|s| {
                s.is_empty()
                    || !s
                        .bytes()
                        .all(|b| b.is_ascii_alphanumeric() || b"-_.".contains(&b))
            })
            || self.application_id.is_empty()
        {
            return Err("Invalid application trust configuration".into());
        }
        if self.keys.is_empty() {
            return Err("Updates are unavailable: this build has no production update key".into());
        }
        for key in self.keys.values() {
            if B64.decode(key)?.len() != 32 {
                return Err("Invalid pinned Ed25519 key".into());
            }
        }
        Ok(())
    }
    pub fn release_url(&self, version: &str) -> String {
        format!(
            "https://github.com/{}/releases/tag/v{}",
            self.repository, version
        )
    }
    pub fn asset_url(&self, version: &str, filename: &str) -> String {
        format!(
            "https://github.com/{}/releases/download/v{}/{}",
            self.repository, version, filename
        )
    }
    pub fn feed_url(&self) -> String {
        format!(
            "https://github.com/{}/releases/latest/download/update-manifest.json",
            self.repository
        )
    }
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Envelope {
    pub key_id: String,
    pub payload: String,
    pub signature: String,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Manifest {
    pub schema: u32,
    pub application_id: String,
    pub repository: String,
    pub version: String,
    pub tag: String,
    pub channel: String,
    pub draft: bool,
    pub published_at: String,
    pub expires_at: u64,
    pub release_notes_url: String,
    pub restart_required: bool,
    pub migration: Option<String>,
    pub artifacts: Vec<Artifact>,
}
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Artifact {
    pub platform: String,
    pub architecture: String,
    pub minimum_os: String,
    pub format: String,
    pub filename: String,
    pub url: String,
    pub size: u64,
    pub sha256: String,
    /// Apple team ID, exact Authenticode subject, or full GPG fingerprint.
    pub signer: String,
    /// Additional archive signature for Sparkle; required for macOS.
    pub sparkle_signature: Option<String>,
}
/// Only this module constructs an authenticated release. Callers cannot turn raw JSON into one.
#[derive(Clone, Debug)]
pub struct VerifiedRelease {
    manifest: Manifest,
}
impl VerifiedRelease {
    pub fn manifest(&self) -> &Manifest {
        &self.manifest
    }
    pub fn candidate(
        &self,
        current: &str,
        platform: &str,
        arch: &str,
        os: &str,
    ) -> Result<Option<&Artifact>> {
        let current = stable_version(current)?;
        if stable_version(&self.manifest.version)? <= current {
            return Ok(None);
        }
        if let Some(instructions) = &self.manifest.migration {
            return Err(format!(
                "This release requires manual migration: {instructions}. See {}",
                self.manifest.release_notes_url
            )
            .into());
        }
        let found = self
            .manifest
            .artifacts
            .iter()
            .find(|a| a.platform == platform && a.architecture == arch);
        match found {
            Some(a) if os_version(os)? >= os_version(&a.minimum_os)? => Ok(Some(a)),
            _ => Ok(None),
        }
    }
}
pub fn stable_version(value: &str) -> Result<Version> {
    let version = Version::parse(value)?;
    if !version.pre.is_empty() || !version.build.is_empty() {
        return Err("Stable versions must be MAJOR.MINOR.PATCH".into());
    }
    Ok(version)
}
fn os_version(value: &str) -> Result<Vec<u64>> {
    let mut parts = value
        .split('.')
        .map(str::parse::<u64>)
        .collect::<std::result::Result<Vec<_>, _>>()?;
    if parts.is_empty() || parts.len() > 4 {
        return Err("Invalid minimum OS".into());
    }
    parts.resize(4, 0);
    Ok(parts)
}
/// Persist timestamps in native preferences. Failed attempts back off; manual checks bypass the clock.
pub fn should_check(
    now: u64,
    last_success: u64,
    last_attempt: u64,
    automatic: bool,
    manual: bool,
) -> bool {
    manual
        || (automatic
            && (last_success == 0 || last_success > now || now - last_success >= CHECK_INTERVAL)
            && (last_attempt == 0 || last_attempt > now || now - last_attempt >= RETRY_INTERVAL))
}
pub fn verify(bytes: &[u8], trust: &Trust, now: u64) -> Result<VerifiedRelease> {
    trust.validate()?;
    if bytes.len() as u64 > MAX_MANIFEST {
        return Err("Update metadata exceeds size limit".into());
    }
    let envelope: Envelope = serde_json::from_slice(bytes)?;
    let key = trust
        .keys
        .get(&envelope.key_id)
        .ok_or("Unknown update signing key")?;
    let payload = B64.decode(&envelope.payload)?;
    UnparsedPublicKey::new(&ED25519, B64.decode(key)?)
        .verify(&payload, &B64.decode(&envelope.signature)?)
        .map_err(|_| "Invalid update signature")?;
    let manifest: Manifest = serde_json::from_slice(&payload)?;
    validate_manifest(&manifest, trust, now)?;
    Ok(VerifiedRelease { manifest })
}
pub fn validate_manifest(m: &Manifest, trust: &Trust, now: u64) -> Result<()> {
    stable_version(&m.version)?;
    let published = chrono::DateTime::parse_from_rfc3339(&m.published_at)?.timestamp();
    if m.schema != 1
        || m.application_id != trust.application_id
        || m.repository != trust.repository
        || m.channel != "stable"
        || m.draft
        || m.tag != format!("v{}", m.version)
        || m.release_notes_url != trust.release_url(&m.version)
        || published < 0
        || published as u64 > now.saturating_add(300)
        || m.expires_at <= now
        || m.expires_at <= published as u64
        || m.expires_at - published as u64 > 90 * CHECK_INTERVAL
        || m.artifacts.is_empty()
    {
        return Err("Invalid identity, stable release metadata, or expired manifest".into());
    }
    let mut targets = std::collections::BTreeSet::new();
    for a in &m.artifacts {
        os_version(&a.minimum_os)?;
        let (format, signer) = match a.platform.as_str() {
            "macos" => ("zip", &trust.macos_team_id),
            "windows" => ("msi", &trust.windows_publisher),
            "linux" => ("AppImage", &trust.linux_gpg_fingerprint),
            _ => return Err("Unknown update platform".into()),
        };
        if !["x64", "arm64"].contains(&a.architecture.as_str())
            || a.format != format
            || signer.is_empty()
            || &a.signer != signer
            || a.filename
                != format!(
                    "{}-{}-{}-{}.{}",
                    trust.repository.split('/').next_back().unwrap(),
                    m.version,
                    a.platform,
                    a.architecture,
                    format
                )
            || a.url != trust.asset_url(&m.version, &a.filename)
            || a.size == 0
            || a.size > 2 * 1024 * 1024 * 1024
            || a.sha256.len() != 64
            || !a
                .sha256
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
            || !targets.insert((&a.platform, &a.architecture))
        {
            return Err(
                "Invalid artifact identity, URL, platform, architecture, size or digest".into(),
            );
        }
        if a.platform == "macos"
            && a.sparkle_signature
                .as_ref()
                .and_then(|s| B64.decode(s).ok())
                .is_none_or(|s| s.len() != 64)
        {
            return Err("Missing Sparkle archive signature".into());
        }
    }
    Ok(())
}
/// The transport follows HTTPS GitHub asset redirects only; it has no token, cookies, or telemetry.
pub fn client() -> Result<reqwest::blocking::Client> {
    Ok(reqwest::blocking::Client::builder()
        .https_only(true)
        .redirect(reqwest::redirect::Policy::custom(|attempt| {
            let host = attempt.url().host_str().unwrap_or("");
            if attempt.previous().len() >= 5
                || attempt.url().scheme() != "https"
                || ![
                    "github.com",
                    "release-assets.githubusercontent.com",
                    "objects.githubusercontent.com",
                ]
                .contains(&host)
            {
                attempt.error("Update redirect is outside GitHub release storage")
            } else {
                attempt.follow()
            }
        }))
        .connect_timeout(Duration::from_secs(10))
        .timeout(Duration::from_secs(900))
        .user_agent("TLO-Labs-Updater/1")
        .build()?)
}
pub fn check(trust: &Trust, now: u64) -> Result<VerifiedRelease> {
    trust.validate()?;
    let response = client()?
        .get(trust.feed_url())
        .timeout(Duration::from_secs(30))
        .send()?
        .error_for_status()?;
    read_release(response, trust, now)
}
/// Bounded stream boundary also used by mock-server qualification.
pub fn read_release(source: impl Read, trust: &Trust, now: u64) -> Result<VerifiedRelease> {
    let mut bytes = Vec::new();
    source.take(MAX_MANIFEST + 1).read_to_end(&mut bytes)?;
    verify(&bytes, trust, now)
}
pub fn copy_verified(
    mut source: impl Read,
    mut dest: impl Write,
    artifact: &Artifact,
) -> Result<()> {
    let mut hash = Sha256::new();
    let mut size = 0u64;
    let mut buffer = [0u8; 65536];
    loop {
        let n = source.read(&mut buffer)?;
        if n == 0 {
            break;
        }
        size += n as u64;
        if size > artifact.size {
            return Err("Download exceeds authenticated size".into());
        }
        hash.update(&buffer[..n]);
        dest.write_all(&buffer[..n])?;
    }
    if size != artifact.size || format!("{:x}", hash.finalize()) != artifact.sha256 {
        return Err("Download checksum or size mismatch".into());
    }
    Ok(())
}
/// Temporary files disappear on failure. An authenticated download is not permission to execute.
pub fn download(
    release: &VerifiedRelease,
    artifact: &Artifact,
    directory: &Path,
) -> Result<tempfile::NamedTempFile> {
    if !release.manifest.artifacts.iter().any(|a| a == artifact) {
        return Err("Artifact is not in the authenticated release".into());
    }
    let mut staged = tempfile::NamedTempFile::new_in(directory)?;
    let response = client()?.get(&artifact.url).send()?.error_for_status()?;
    copy_verified(response, &mut staged, artifact)?;
    staged.as_file().sync_all()?;
    Ok(staged)
}
#[cfg(unix)]
pub mod appimage;
