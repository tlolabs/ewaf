// SPDX-License-Identifier: GPL-3.0-or-later
use std::time::{SystemTime, UNIX_EPOCH};
use tlo_updater::{Result, Trust};
fn staging_directory(path: &std::path::Path) -> Result<std::path::PathBuf> {
    let info = std::fs::symlink_metadata(path)?;
    if !path.is_absolute() || !info.is_dir() || info.file_type().is_symlink() {
        return Err("Staging directory must be an absolute ordinary directory".into());
    }
    // Preserve the caller's Windows spelling for MSI APIs. canonicalize() adds
    // a device-path prefix, which is not an MSI compatibility guarantee. The
    // native verifier locks/checks every ancestor before package use.
    #[cfg(windows)]
    return Ok(path.to_owned());
    #[cfg(not(windows))]
    Ok(path.canonicalize()?)
}

fn run() -> Result<serde_json::Value> {
    let args: Vec<String> = std::env::args().collect();
    let command = args.get(1).map(String::as_str).unwrap_or("check");
    let now = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs();
    if command == "due" {
        let n = |i: usize| args.get(i).and_then(|v| v.parse().ok()).unwrap_or(0);
        return Ok(
            serde_json::json!({"due":tlo_updater::should_check(now,n(2),n(3),n(4)!=0,false)}),
        );
    }
    if command == "version" {
        return Ok(serde_json::json!({"version":env!("CARGO_PKG_VERSION")}));
    }
    if option_env!("EWAF_UPDATE_CHANNEL") != Some("stable") {
        return Err("Updates are disabled in this development build; install an official stable EWAF release".into());
    }
    let trust: Trust = serde_json::from_str(include_str!("../../../updates/trust.json"))?;
    if !["check", "download", "install-appimage"].contains(&command) {
        return Err("Unknown updater command".into());
    }
    let os = args.get(2).ok_or("OS version is required")?;
    let platform = match std::env::consts::OS {
        "macos" => "macos",
        "windows" => "windows",
        "linux" => "linux",
        _ => return Err("Unsupported OS".into()),
    };
    let arch = match std::env::consts::ARCH {
        "aarch64" => "arm64",
        "x86_64" => "x64",
        _ => return Err("Unsupported architecture".into()),
    };
    let release = tlo_updater::check(&trust, now)?;
    let candidate = release.candidate(env!("CARGO_PKG_VERSION"), platform, arch, os)?;
    let m = release.manifest();
    if command == "check" {
        return Ok(
            serde_json::json!({"available":candidate.is_some(),"version":m.version,"notes":m.release_notes_url,"migration":m.migration,"checked_at":now}),
        );
    }
    if args.get(3) != Some(&m.version) {
        return Err("Release changed since confirmation; check again".into());
    }
    let artifact = candidate.ok_or("No compatible newer stable update")?;
    if command == "install-appimage" {
        #[cfg(target_os = "linux")]
        {
            let target = std::env::var("APPIMAGE")
                .map_err(|_| "Use your package manager: this is not an AppImage installation")?;
            let backup =
                tlo_updater::appimage::install(&release, artifact, std::path::Path::new(&target))?;
            return Ok(serde_json::json!({"installed":true,"version":m.version,"backup":backup}));
        }
        #[cfg(not(target_os = "linux"))]
        return Err("AppImage installation is supported only on Linux".into());
    }
    // The UI can own staging before launch, so cancellation and process failure
    // cannot strand a kept temporary directory whose path was never returned.
    let owned_directory = if args.get(4).is_none() {
        Some(tempfile::Builder::new().prefix("ewaf-update-").tempdir()?)
    } else {
        None
    };
    let directory = match args.get(4) {
        Some(path) => std::path::Path::new(path),
        None => owned_directory.as_ref().unwrap().path(),
    };
    let directory = staging_directory(directory)?;
    let staged = tlo_updater::download(&release, artifact, &directory)?;
    let path = directory.join(&artifact.filename);
    staged.persist_noclobber(&path)?;
    if let Some(directory) = owned_directory {
        let _ = directory.keep();
    }
    Ok(
        serde_json::json!({"path":path,"version":m.version,"sha256":artifact.sha256,"signer":artifact.signer,"architecture":artifact.architecture,"notes":m.release_notes_url}),
    )
}
fn main() {
    match run() {
        Ok(value) => println!("{value}"),
        Err(error) => {
            println!("{}", serde_json::json!({"error":error.to_string()}));
            std::process::exit(1);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn caller_owned_staging_accepts_native_absolute_directory() {
        let directory = tempfile::tempdir().unwrap();
        let validated = staging_directory(directory.path()).unwrap();
        assert_eq!(
            validated.canonicalize().unwrap(),
            directory.path().canonicalize().unwrap()
        );
        #[cfg(windows)]
        assert_eq!(validated, directory.path());
        let file = directory.path().join("file");
        std::fs::write(&file, b"not a directory").unwrap();
        assert!(staging_directory(&file).is_err());
        assert!(staging_directory(std::path::Path::new(".")).is_err());
    }
    #[cfg(unix)]
    #[test]
    fn staging_does_not_accept_a_directory_symlink() {
        let directory = tempfile::tempdir().unwrap();
        let link = directory.path().join("alias");
        std::os::unix::fs::symlink(directory.path(), &link).unwrap();
        assert!(staging_directory(&link).is_err());
    }
}
