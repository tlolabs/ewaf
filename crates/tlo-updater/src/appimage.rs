// SPDX-License-Identifier: GPL-3.0-or-later
use crate::{Artifact, Result, VerifiedRelease};
use std::{
    fs,
    os::unix::fs::{MetadataExt, PermissionsExt},
    path::{Path, PathBuf},
};
/// User-authorized replacement on the same filesystem; keeps an executable backup.
/// A stable lock and durable version floor also protect against old running instances.
pub fn install(release: &VerifiedRelease, artifact: &Artifact, target: &Path) -> Result<PathBuf> {
    if artifact.platform != "linux" || artifact.format != "AppImage" || !target.is_absolute() {
        return Err(
            "This installation requires a writable AppImage; use the package manager otherwise"
                .into(),
        );
    }
    if !release.manifest().artifacts.iter().any(|a| a == artifact) {
        return Err("Artifact is not in the authenticated release".into());
    }
    let transaction = Transaction::begin(target, &release.manifest().version, artifact)?;
    let staged = crate::download(release, artifact, &transaction.parent)?;
    transaction.commit(staged, artifact)
}

#[derive(serde::Serialize, serde::Deserialize)]
#[serde(deny_unknown_fields)]
struct VersionFloor {
    version: String,
    sha256: String,
}

fn safe_ancestor(owner: u32, mode: u32, expected_uid: u32) -> bool {
    // Sticky directories protect their user-owned direct children, including
    // /tmp and /run/media. A root-owned group-writable nonsticky directory does
    // not: members of that group can rename the next path component.
    (owner == 0 || owner == expected_uid) && (mode & 0o022 == 0 || mode & 0o1000 != 0)
}

fn verify_safe_ancestors(parent: &Path, expected_uid: u32) -> Result<()> {
    for ancestor in parent.ancestors().skip(1) {
        let meta = fs::symlink_metadata(ancestor)?;
        if !meta.is_dir()
            || meta.file_type().is_symlink()
            || !safe_ancestor(meta.uid(), meta.mode(), expected_uid)
        {
            return Err(
                "AppImage ancestor must be a trusted directory, non-writable by others or sticky"
                    .into(),
            );
        }
    }
    Ok(())
}

struct Transaction {
    target: PathBuf,
    initial: fs::Metadata,
    parent: PathBuf,
    original_parent: PathBuf,
    directory: fs::File,
    // Never unlink this file: all running instances must lock the same inode.
    _lock: fs::File,
    state: PathBuf,
    floor: VersionFloor,
}
impl Transaction {
    fn begin(target: &Path, version: &str, artifact: &Artifact) -> Result<Self> {
        let next = crate::stable_version(version)?;
        let initial = fs::symlink_metadata(target)?;
        if !target.is_absolute()
            || !initial.is_file()
            || initial.file_type().is_symlink()
            || initial.permissions().mode() & 0o022 != 0
        {
            return Err(
                "AppImage must be an absolute regular file writable only by its owner".into(),
            );
        }
        let parent = target.parent().ok_or("Missing AppImage parent")?;
        if parent.canonicalize()? != parent {
            return Err("AppImage parent must not contain symbolic links".into());
        }
        let directory = fs::File::open(parent)?;
        let parent_info = directory.metadata()?;
        if parent_info.permissions().mode() & 0o022 != 0 || parent_info.uid() != initial.uid() {
            return Err("AppImage directory must be owned by the application owner and not writable by others".into());
        }
        verify_safe_ancestors(parent, initial.uid())?;
        let original_parent = parent.to_path_buf();
        // Linux transactions resolve every mutable child through the open
        // directory capability. A renamed ancestor cannot redirect a commit.
        #[cfg(target_os = "linux")]
        let parent = {
            use std::os::fd::AsRawFd;
            PathBuf::from(format!("/proc/self/fd/{}", directory.as_raw_fd()))
        };
        #[cfg(not(target_os = "linux"))]
        let parent = parent.to_path_buf();
        let anchored_target = parent.join(target.file_name().ok_or("Missing AppImage filename")?);
        let anchored_info = fs::symlink_metadata(&anchored_target)?;
        if anchored_info.dev() != initial.dev() || anchored_info.ino() != initial.ino() {
            return Err("AppImage parent changed while opening transaction".into());
        }
        let name = target
            .file_name()
            .ok_or("Missing AppImage filename")?
            .to_string_lossy();
        let lock_path = parent.join(format!(".{name}.tlo-update.lock"));
        use std::os::unix::fs::OpenOptionsExt;
        let lock = match fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create_new(true)
            .mode(0o600)
            .open(&lock_path)
        {
            Ok(file) => file,
            Err(error) if error.kind() == std::io::ErrorKind::AlreadyExists => {
                let metadata = fs::symlink_metadata(&lock_path)?;
                if !metadata.is_file()
                    || metadata.file_type().is_symlink()
                    || metadata.uid() != initial.uid()
                    || metadata.mode() & 0o077 != 0
                {
                    return Err("Unsafe AppImage transaction lock".into());
                }
                let file = fs::OpenOptions::new()
                    .read(true)
                    .write(true)
                    .open(&lock_path)?;
                if file.metadata()?.ino() != metadata.ino()
                    || file.metadata()?.dev() != metadata.dev()
                {
                    return Err("AppImage transaction lock changed".into());
                }
                file
            }
            Err(error) => return Err(error.into()),
        };
        lock.try_lock()
            .map_err(|_| "Another update is already using this AppImage")?;
        let state = parent.join(format!(".{name}.tlo-update.json"));
        match fs::symlink_metadata(&state) {
            Ok(metadata) => {
                if !metadata.is_file()
                    || metadata.file_type().is_symlink()
                    || metadata.uid() != initial.uid()
                    || metadata.mode() & 0o077 != 0
                    || metadata.len() > 4096
                {
                    return Err("Unsafe AppImage version journal".into());
                }
                let floor: VersionFloor = serde_json::from_slice(&fs::read(&state)?)?;
                let minimum = crate::stable_version(&floor.version)?;
                if next < minimum || (next == minimum && artifact.sha256 != floor.sha256) {
                    return Err("A newer update was already installed or prepared; reopen EWAF and check again".into());
                }
                if next == minimum
                    && crate::copy_verified(fs::File::open(target)?, std::io::sink(), artifact)
                        .is_ok()
                {
                    return Err("This update is already installed; reopen EWAF".into());
                }
            }
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => {}
            Err(error) => return Err(error.into()),
        }
        Ok(Self {
            target: anchored_target,
            parent,
            original_parent,
            initial,
            directory,
            _lock: lock,
            state,
            floor: VersionFloor {
                version: version.into(),
                sha256: artifact.sha256.clone(),
            },
        })
    }

    fn verify_directory(&self) -> Result<()> {
        let held = self.directory.metadata()?;
        let current = fs::symlink_metadata(&self.original_parent)?;
        if !current.is_dir()
            || current.file_type().is_symlink()
            || current.dev() != held.dev()
            || current.ino() != held.ino()
            || held.uid() != self.initial.uid()
            || held.mode() & 0o022 != 0
            || self.original_parent.canonicalize()? != self.original_parent
        {
            return Err("AppImage directory changed or became unsafe while updating".into());
        }
        verify_safe_ancestors(&self.original_parent, self.initial.uid())
    }

    fn commit(self, staged: tempfile::NamedTempFile, artifact: &Artifact) -> Result<PathBuf> {
        self.commit_with_hook(staged, artifact, || {})
    }

    fn commit_with_hook(
        self,
        mut staged: tempfile::NamedTempFile,
        artifact: &Artifact,
        before_journal: impl FnOnce(),
    ) -> Result<PathBuf> {
        use std::io::{Read, Seek, Write};
        self.verify_directory()?;
        let target = &self.target;
        let initial = &self.initial;
        let parent = target.parent().unwrap();
        let mut original = fs::File::open(target)?;
        original
            .try_lock()
            .map_err(|_| "Another update is already using this AppImage")?;
        let mut header = [0u8; 12];
        original.read_exact(&mut header)?;
        if &header[..4] != b"\x7fELF" || &header[8..11] != b"AI\x02" {
            return Err("The running application path is not a type-2 AppImage".into());
        }
        original.rewind()?;
        if original.metadata()?.ino() != initial.ino()
            || original.metadata()?.dev() != initial.dev()
        {
            return Err("AppImage changed while updating".into());
        }
        let current = original.metadata()?;
        if current.uid() != initial.uid() || current.mode() & 0o022 != 0 {
            return Err("AppImage owner or permissions changed while updating".into());
        }
        // Authenticate the staged bytes again before retaining anything or advancing the journal.
        crate::copy_verified(fs::File::open(staged.path())?, std::io::sink(), artifact)?;
        let mut next_header = [0u8; 20];
        staged.as_file_mut().rewind()?;
        staged.as_file_mut().read_exact(&mut next_header)?;
        let machine = match artifact.architecture.as_str() {
            "x64" => 62u16,
            "arm64" => 183u16,
            _ => return Err("Unsupported AppImage architecture".into()),
        };
        if &next_header[..7] != b"\x7fELF\x02\x01\x01"
            || &next_header[8..11] != b"AI\x02"
            || u16::from_le_bytes([next_header[18], next_header[19]]) != machine
        {
            return Err(
                "Downloaded AppImage type or architecture does not match the authenticated release"
                    .into(),
            );
        }
        staged
            .as_file()
            .set_permissions(fs::Permissions::from_mode(initial.mode() & 0o777))?;
        let mut previous = tempfile::Builder::new()
            .prefix("ewaf-previous-")
            .suffix(".AppImage")
            .tempfile_in(parent)?;
        std::io::copy(&mut original, &mut previous)?;
        previous.as_file().set_permissions(initial.permissions())?;
        previous.as_file().sync_all()?;
        let (_, backup) = previous.keep()?;
        self.directory.sync_all()?;
        let current = fs::symlink_metadata(target)?;
        if current.ino() != initial.ino()
            || current.dev() != initial.dev()
            || current.len() != initial.len()
            || current.mtime() != initial.mtime()
            || current.mtime_nsec() != initial.mtime_nsec()
        {
            return Err("AppImage changed while updating; previous image retained".into());
        }
        staged.as_file_mut().sync_all()?;
        // Write ahead: a crash can leave the old image plus a newer floor, never a newer
        // image without its floor. Retrying these exact version/digest bytes is allowed.
        // A no-op in production; tests inject a directory swap after all file
        // identity/digest checks but immediately before the first commit write.
        before_journal();
        self.verify_directory()?;
        let mut journal = tempfile::NamedTempFile::new_in(parent)?;
        journal.write_all(&serde_json::to_vec(&self.floor)?)?;
        journal.as_file().sync_all()?;
        journal.persist(&self.state)?;
        self.directory.sync_all()?;
        self.verify_directory()?;
        staged.persist(target)?;
        self.directory.sync_all()?;
        Ok(backup)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use sha2::{Digest, Sha256};
    use std::io::Write;
    fn image(payload: &[u8]) -> Vec<u8> {
        let mut image =
            b"\x7fELF\x02\x01\x01\x00AI\x02\x00\x00\x00\x00\x00\x02\x00\x3e\x00".to_vec();
        image.extend_from_slice(payload);
        image
    }
    fn fixture() -> (tempfile::TempDir, PathBuf, Artifact) {
        let directory = tempfile::tempdir().unwrap();
        let target = directory
            .path()
            .canonicalize()
            .unwrap()
            .join("EWAF.AppImage");
        fs::write(&target, b"\x7fELF\x02\x01\x01\x00AI\x02\x00old").unwrap();
        fs::set_permissions(&target, fs::Permissions::from_mode(0o755)).unwrap();
        let a = Artifact {
            platform: "linux".into(),
            architecture: "x64".into(),
            minimum_os: "2.39".into(),
            format: "AppImage".into(),
            filename: "test.AppImage".into(),
            url: String::new(),
            size: image(b"new").len() as u64,
            sha256: format!("{:x}", Sha256::digest(image(b"new"))),
            signer: "test".into(),
            sparkle_signature: None,
        };
        (directory, target, a)
    }
    #[test]
    fn replacement_retains_exact_old_executable() {
        let (_directory, target, a) = fixture();
        let old = fs::read(&target).unwrap();
        let mut staged = tempfile::NamedTempFile::new_in(target.parent().unwrap()).unwrap();
        staged.write_all(&image(b"new")).unwrap();
        let backup = Transaction::begin(&target, "2.0.0", &a)
            .unwrap()
            .commit(staged, &a)
            .unwrap();
        assert_eq!(fs::read(&target).unwrap(), image(b"new"));
        assert_eq!(fs::read(backup).unwrap(), old);
    }
    #[test]
    fn interrupted_staging_and_corruption_leave_old_image() {
        let (_directory, target, a) = fixture();
        let old = fs::read(&target).unwrap();
        let mut staged = tempfile::NamedTempFile::new_in(target.parent().unwrap()).unwrap();
        staged.write_all(b"ne").unwrap();
        drop(staged);
        assert_eq!(fs::read(&target).unwrap(), old);
        let mut staged = tempfile::NamedTempFile::new_in(target.parent().unwrap()).unwrap();
        staged.write_all(b"bad").unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a)
            .unwrap()
            .commit(staged, &a)
            .is_err());
        assert_eq!(fs::read(&target).unwrap(), old);
    }
    #[test]
    fn unrelated_file_and_symlink_are_preserved() {
        let (_directory, target, a) = fixture();
        fs::write(&target, b"unrelated user file").unwrap();
        let old = fs::read(&target).unwrap();
        let staged = tempfile::NamedTempFile::new_in(target.parent().unwrap()).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a)
            .unwrap()
            .commit(staged, &a)
            .is_err());
        assert_eq!(fs::read(&target).unwrap(), old);
        let link = target.with_extension("link");
        std::os::unix::fs::symlink(&target, &link).unwrap();
        assert!(Transaction::begin(&link, "2.0.0", &a).is_err());
        assert_eq!(fs::read(&target).unwrap(), old);
    }
    fn stage(target: &Path, bytes: &[u8]) -> tempfile::NamedTempFile {
        let mut file = tempfile::NamedTempFile::new_in(target.parent().unwrap()).unwrap();
        file.write_all(bytes).unwrap();
        file
    }
    #[test]
    fn newer_installation_blocks_older_running_instances_and_repeat_install() {
        let (_directory, target, a) = fixture();
        Transaction::begin(&target, "3.0.0", &a)
            .unwrap()
            .commit(stage(&target, &image(b"new")), &a)
            .unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a).is_err());
        assert!(Transaction::begin(&target, "3.0.0", &a).is_err());
        assert_eq!(fs::read(&target).unwrap(), image(b"new"));
        let mut changed = a.clone();
        changed.sha256 = "0".repeat(64);
        assert!(Transaction::begin(&target, "3.0.0", &changed).is_err());
        assert!(Transaction::begin(&target, "4.0.0", &a).is_ok());
    }
    #[test]
    fn write_ahead_journal_allows_only_exact_retry_after_interruption() {
        let (_directory, target, a) = fixture();
        let transaction = Transaction::begin(&target, "3.0.0", &a).unwrap();
        // Simulate interruption after a durable journal write but before image replacement.
        fs::write(
            &transaction.state,
            serde_json::to_vec(&transaction.floor).unwrap(),
        )
        .unwrap();
        fs::set_permissions(&transaction.state, fs::Permissions::from_mode(0o600)).unwrap();
        drop(transaction);
        assert!(Transaction::begin(&target, "2.0.0", &a).is_err());
        Transaction::begin(&target, "3.0.0", &a)
            .unwrap()
            .commit(stage(&target, &image(b"new")), &a)
            .unwrap();
        assert_eq!(fs::read(&target).unwrap(), image(b"new"));
    }
    #[test]
    fn concurrent_installer_and_changed_image_preserve_current_bytes() {
        let (_directory, target, a) = fixture();
        let transaction = Transaction::begin(&target, "2.0.0", &a).unwrap();
        assert!(Transaction::begin(&target, "3.0.0", &a).is_err());
        let changed = image(b"another installed image");
        stage(&target, &changed).persist(&target).unwrap();
        assert!(transaction
            .commit(stage(&target, &image(b"new")), &a)
            .is_err());
        assert_eq!(fs::read(&target).unwrap(), changed);
    }
    #[test]
    fn locked_original_refuses_replacement() {
        let (_directory, target, a) = fixture();
        let original = fs::File::open(&target).unwrap();
        original.try_lock().unwrap();
        let bytes = fs::read(&target).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a)
            .unwrap()
            .commit(stage(&target, &image(b"new")), &a)
            .is_err());
        assert_eq!(fs::read(&target).unwrap(), bytes);
    }
    #[test]
    fn unsafe_permissions_and_journal_fail_closed() {
        let (directory, target, a) = fixture();
        fs::set_permissions(&target, fs::Permissions::from_mode(0o777)).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a).is_err());
        fs::set_permissions(&target, fs::Permissions::from_mode(0o755)).unwrap();
        fs::set_permissions(directory.path(), fs::Permissions::from_mode(0o777)).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a).is_err());
        fs::set_permissions(directory.path(), fs::Permissions::from_mode(0o700)).unwrap();
        let transaction = Transaction::begin(&target, "2.0.0", &a).unwrap();
        fs::write(&transaction.state, b"broken journal").unwrap();
        fs::set_permissions(&transaction.state, fs::Permissions::from_mode(0o600)).unwrap();
        drop(transaction);
        assert!(Transaction::begin(&target, "2.0.0", &a).is_err());
    }
    #[test]
    fn authenticated_wrong_architecture_or_type_is_not_installed() {
        let (_directory, target, a) = fixture();
        let old = fs::read(&target).unwrap();
        let mut wrong_arch = a.clone();
        wrong_arch.architecture = "arm64".into();
        assert!(Transaction::begin(&target, "2.0.0", &wrong_arch)
            .unwrap()
            .commit(stage(&target, &image(b"new")), &wrong_arch)
            .is_err());
        let mut wrong_type = a.clone();
        let bytes = vec![0; 30];
        wrong_type.size = bytes.len() as u64;
        wrong_type.sha256 = format!("{:x}", Sha256::digest(&bytes));
        assert!(Transaction::begin(&target, "2.0.0", &wrong_type)
            .unwrap()
            .commit(stage(&target, &bytes), &wrong_type)
            .is_err());
        assert_eq!(fs::read(&target).unwrap(), old);
    }
    #[test]
    fn ancestor_policy_supports_private_homes_system_paths_sticky_and_mounts() {
        for (owner, mode) in [
            (1000, 0o700),
            (1000, 0o755),
            (0, 0o755),
            (0, 0o1777),
            (0, 0o1770),
            (1000, 0o1770),
        ] {
            assert!(
                safe_ancestor(owner, mode, 1000),
                "owner={owner} mode={mode:o}"
            );
        }
        for (owner, mode) in [
            (1001, 0o755),
            (0, 0o775),
            (1000, 0o775),
            (0, 0o777),
            (1000, 0o777),
        ] {
            assert!(
                !safe_ancestor(owner, mode, 1000),
                "owner={owner} mode={mode:o}"
            );
        }
    }

    #[test]
    fn real_ancestor_permissions_and_symlinks_fail_closed() {
        let (directory, _, a) = fixture();
        let ancestor = directory.path().canonicalize().unwrap().join("ancestor");
        let parent = ancestor.join("private");
        fs::create_dir_all(&parent).unwrap();
        let target = parent.join("EWAF.AppImage");
        fs::write(&target, image(b"old")).unwrap();
        fs::set_permissions(&target, fs::Permissions::from_mode(0o755)).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a).is_ok());
        fs::set_permissions(&ancestor, fs::Permissions::from_mode(0o777)).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a).is_err());
        fs::set_permissions(&ancestor, fs::Permissions::from_mode(0o1777)).unwrap();
        assert!(Transaction::begin(&target, "2.0.0", &a).is_ok());
        let alias = directory.path().join("alias");
        std::os::unix::fs::symlink(&ancestor, &alias).unwrap();
        assert!(Transaction::begin(&alias.join("private/EWAF.AppImage"), "2.0.0", &a).is_err());
    }

    #[test]
    fn renamed_parent_and_permission_changes_do_not_redirect_commit() {
        let (directory, target, a) = fixture();
        let parent = target.parent().unwrap();
        let transaction = Transaction::begin(&target, "2.0.0", &a).unwrap();
        fs::set_permissions(parent, fs::Permissions::from_mode(0o777)).unwrap();
        assert!(transaction
            .commit(stage(&target, &image(b"new")), &a)
            .is_err());
        fs::set_permissions(parent, fs::Permissions::from_mode(0o700)).unwrap();
        let nested = parent.join("nested");
        fs::create_dir(&nested).unwrap();
        let nested_target = nested.join("EWAF.AppImage");
        fs::copy(&target, &nested_target).unwrap();
        let transaction = Transaction::begin(&nested_target, "2.0.0", &a).unwrap();
        let staged = stage(&nested_target, &image(b"new"));
        let moved = directory.path().join("moved");
        fs::rename(&nested, &moved).unwrap();
        fs::create_dir(&nested).unwrap();
        fs::write(&nested_target, b"unrelated user data").unwrap();
        assert!(transaction.commit(staged, &a).is_err());
        assert_eq!(fs::read(&nested_target).unwrap(), b"unrelated user data");
        assert_eq!(
            fs::read(moved.join("EWAF.AppImage")).unwrap(),
            fs::read(&target).unwrap()
        );
    }
    #[test]
    fn final_ancestor_swap_cannot_overwrite_an_unrelated_replacement_target() {
        let (directory, target, a) = fixture();
        let parent = target.parent().unwrap().join("application");
        fs::create_dir(&parent).unwrap();
        let target = parent.join("EWAF.AppImage");
        fs::write(&target, image(b"old")).unwrap();
        fs::set_permissions(&target, fs::Permissions::from_mode(0o755)).unwrap();
        let original = fs::read(&target).unwrap();
        let transaction = Transaction::begin(&target, "2.0.0", &a).unwrap();
        let staged = stage(&target, &image(b"new"));
        let staged_name = staged.path().file_name().unwrap().to_owned();
        let moved = directory.path().join("application-moved");
        let result = transaction.commit_with_hook(staged, &a, || {
            fs::rename(&parent, &moved).unwrap();
            fs::create_dir(&parent).unwrap();
            fs::write(&target, b"unrelated replacement-directory data").unwrap();
            // Recreate the known staged pathname. Rechecking only the original
            // image's inode/digest before this swap would still overwrite target.
            fs::write(parent.join(staged_name), image(b"new")).unwrap();
        });
        assert!(result.is_err());
        assert_eq!(
            fs::read(&target).unwrap(),
            b"unrelated replacement-directory data"
        );
        assert_eq!(fs::read(moved.join("EWAF.AppImage")).unwrap(), original);
    }
}
