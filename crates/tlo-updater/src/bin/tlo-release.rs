// SPDX-License-Identifier: GPL-3.0-or-later
//! Offline signing/qualification and real published-channel download verification.
use base64::{engine::general_purpose::STANDARD as B64, Engine};
use ring::signature::{Ed25519KeyPair, KeyPair};
use std::{
    fs,
    path::Path,
    time::{SystemTime, UNIX_EPOCH},
};
use tlo_updater::{Envelope, Manifest, Result, Trust};
fn run() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    let command = args
        .get(1)
        .ok_or("Usage: tlo-release sign|verify|published TRUST INPUT [OUTPUT|OLDER_VERSION]")?;
    let trust: Trust =
        serde_json::from_slice(&fs::read(args.get(2).ok_or("Missing trust file")?)?)?;
    trust.validate()?;
    let now = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
    if command == "sign" {
        let payload = fs::read(args.get(3).ok_or("Missing payload")?)?;
        let m: Manifest = serde_json::from_slice(&payload)?;
        tlo_updater::validate_manifest(&m, &trust, now)?;
        let seed = std::env::var("TLO_UPDATE_SIGNING_SEED")
            .map_err(|_| "Missing protected Ed25519 seed")?;
        let key = Ed25519KeyPair::from_seed_unchecked(&B64.decode(seed)?)
            .map_err(|_| "Invalid signing seed")?;
        let id = trust
            .keys
            .iter()
            .find(|(_, public)| {
                B64.decode(public).ok().as_deref() == Some(key.public_key().as_ref())
            })
            .map(|(id, _)| id)
            .ok_or("Signing key is not pinned in this application")?;
        let envelope = Envelope {
            key_id: id.clone(),
            payload: B64.encode(&payload),
            signature: B64.encode(key.sign(&payload)),
        };
        let bytes = serde_json::to_vec(&envelope)?;
        tlo_updater::verify(&bytes, &trust, now)?;
        fs::write(args.get(4).ok_or("Missing output")?, bytes)?;
    } else if command == "verify" {
        let input = Path::new(args.get(3).ok_or("Missing manifest")?);
        let release = tlo_updater::verify(&fs::read(input)?, &trust, now)?;
        for artifact in &release.manifest().artifacts {
            tlo_updater::copy_verified(
                fs::File::open(input.parent().unwrap().join(&artifact.filename))?,
                std::io::sink(),
                artifact,
            )?;
        }
        println!(
            "Authenticated manifest and all local artifact bytes: {}",
            release.manifest().version
        );
    } else if command == "published" {
        let older = args.get(3).ok_or("Missing older installed version")?;
        let release = tlo_updater::check(&trust, now)?;
        if tlo_updater::stable_version(older)?
            >= tlo_updater::stable_version(&release.manifest().version)?
        {
            return Err("Qualification requires an older client version".into());
        }
        let directory = tempfile::tempdir()?;
        for artifact in &release.manifest().artifacts {
            if release
                .candidate(
                    older,
                    &artifact.platform,
                    &artifact.architecture,
                    &artifact.minimum_os,
                )?
                .is_none()
            {
                return Err("Older client cannot discover update".into());
            }
            tlo_updater::download(&release, artifact, directory.path())?;
            println!(
                "Discovered, authenticated and downloaded {} (installation NOT qualified)",
                artifact.filename
            );
        }
    } else {
        return Err("Unknown qualification command".into());
    }
    Ok(())
}
fn main() {
    if let Err(e) = run() {
        eprintln!("Release validation failed: {e}");
        std::process::exit(1);
    }
}
