use ewaf_core::*;
use serde::Deserialize;
use std::fs;
fn request(start: &str, end: &str, weekday: u32) -> PlanRequest {
    PlanRequest {
        start: start.into(),
        end: end.into(),
        weekday,
    }
}
fn weekly() -> Plan {
    Plan::new(&request("09-03-2026", "09-17-2026", 5)).unwrap()
}
#[test]
fn reference_fixtures() {
    #[derive(Deserialize)]
    struct Fixture {
        start: String,
        end: String,
        weekday: u32,
        expected: Vec<String>,
    }
    let fixtures: Vec<Fixture> = serde_json::from_str(include_str!(
        "../../../tests/EWAFCoreTests/Fixtures/calendar-dates.json"
    ))
    .unwrap();
    assert_eq!(fixtures.len(), 112);
    for f in fixtures {
        assert_eq!(
            Plan::new(&request(&f.start, &f.end, f.weekday))
                .unwrap()
                .preview("", usize::MAX),
            f.expected
        );
    }
}
#[test]
fn malformed_and_calendar_validation() {
    for s in [
        "",
        "..",
        "../09-03-2026",
        "09-03-2026/child",
        "09-03-2026\0",
        "9-3-2026",
        "09-03-26",
        " 09-03-2026",
        "０９-０３-２０２６",
        "02-29-1900",
        "02-29-1500",
        "02-29-2100",
        "02-30-2024",
        "13-01-2024",
        "00-01-2024",
        "01-00-2024",
        "01-01-0000",
    ] {
        assert!(CivilDate::parse(s).is_err(), "{s}");
    }
    for s in ["02-29-2000", "02-29-2024", "01-01-0001", "12-31-9999"] {
        assert_eq!(CivilDate::parse(s).unwrap().name(), s);
    }
    assert!(CivilDate::new(i32::MAX, u32::MAX, u32::MAX).is_err());
    assert_eq!(
        Plan::new(&request("09-17-2026", "09-03-2026", 5))
            .unwrap_err()
            .code,
        "reversed_range"
    );
    for w in [0, 8, u32::MAX] {
        assert_eq!(
            Plan::new(&request("09-03-2026", "09-17-2026", w))
                .unwrap_err()
                .code,
            "invalid_weekday"
        );
    }
}
#[test]
fn boundaries_ordering_and_weekdays() {
    let min = CivilDate::parse("01-01-0001").unwrap();
    let max = CivilDate::parse("12-31-9999").unwrap();
    assert_eq!(min.weekday(), 2);
    assert_eq!(max.weekday(), 6);
    assert!(min.adding(-1).is_none());
    assert!(max.adding(1).is_none());
    assert!(max.adding(i32::MAX).is_none());
    assert_eq!(
        Plan::new(&request("12-31-9999", "12-31-9999", 7))
            .unwrap()
            .dates
            .len(),
        0
    );
    for weekday in 1..=7 {
        let plan = Plan::new(&request("01-01-0001", "12-31-9999", weekday)).unwrap();
        assert!(plan.dates.iter().all(|d| d.weekday() == weekday));
        assert!(plan.dates.windows(2).all(|p| p[0].adding(7) == Some(p[1])));
        if weekday == 5 {
            assert_eq!(plan.dates.len(), 521_723);
            assert_eq!(plan.dates[0].name(), "01-04-0001");
            assert_eq!(plan.dates.last().unwrap().name(), "12-30-9999");
        }
    }
}
#[test]
fn preview_and_confirmation() {
    let start = CivilDate::parse("01-01-2026").unwrap();
    for (count, confirmation) in [(250, false), (251, true)] {
        let plan = Plan::new(&request(
            &start.name(),
            &start.adding(7 * (count - 1)).unwrap().name(),
            5,
        ))
        .unwrap();
        assert_eq!(plan.requires_confirmation(), confirmation);
        assert_eq!(plan.preview("", PREVIEW_LIMIT).len(), 200);
        assert!(plan.preview("nonsense", 200).is_empty());
        assert!(plan.preview("", 0).is_empty());
    }
    assert_eq!(weekly().preview("０９-１０", 200), vec!["09-10-2026"]);
    assert_eq!(weekly().preview("09-10", 200), vec!["09-10-2026"]);
    assert_eq!(weekly().dates.len(), 3);
}
#[test]
fn create_preserve_retry_and_file_conflict() {
    let dir = tempfile::tempdir().unwrap();
    fs::create_dir(dir.path().join("09-03-2026")).unwrap();
    fs::write(dir.path().join("09-03-2026/keep"), b"keep my work").unwrap();
    fs::write(dir.path().join("09-10-2026"), b"collision").unwrap();
    let result = Creation::begin(weekly(), dir.path()).unwrap().step(false);
    assert_eq!(result.existing, 1);
    assert_eq!(result.created, 0);
    assert_eq!(result.error_code.as_deref(), Some("conflict"));
    assert!(!dir.path().join("09-17-2026").exists());
    assert_eq!(
        fs::read(dir.path().join("09-10-2026")).unwrap(),
        b"collision"
    );
    fs::rename(dir.path().join("09-10-2026"), dir.path().join("saved-file")).unwrap();
    let result = Creation::begin(weekly(), dir.path()).unwrap().step(false);
    assert_eq!(result.existing, 1);
    assert_eq!(result.created, 2);
    assert!(result.done);
    assert_eq!(
        fs::read(dir.path().join("09-03-2026/keep")).unwrap(),
        b"keep my work"
    );
    assert_eq!(
        Creation::begin(weekly(), dir.path())
            .unwrap()
            .step(false)
            .existing,
        3
    );
}
#[test]
fn cancellation_terminal_state_and_retry() {
    let dir = tempfile::tempdir().unwrap();
    let plan = Plan::new(&request("01-01-2020", "12-31-2030", 5)).unwrap();
    let mut creation = Creation::begin(plan.clone(), dir.path()).unwrap();
    assert_eq!(creation.step(false).created, 25);
    let cancelled = creation.step(true);
    assert_eq!(cancelled.created, 25);
    assert!(cancelled.cancelled && cancelled.done);
    assert_eq!(creation.step(false), cancelled);
    let mut retry = Creation::begin(plan.clone(), dir.path()).unwrap();
    let mut result = retry.step(false);
    while !result.done {
        result = retry.step(false);
    }
    assert_eq!(result.existing, 25);
    assert_eq!(result.processed(), plan.dates.len());
    let empty = tempfile::tempdir().unwrap();
    assert!(
        Creation::begin(weekly(), empty.path())
            .unwrap()
            .step(true)
            .cancelled
    );
    assert_eq!(fs::read_dir(empty.path()).unwrap().count(), 0);
}
#[test]
fn destination_and_empty_plan() {
    let dir = tempfile::tempdir().unwrap();
    assert!(Creation::begin(weekly(), &dir.path().join("missing")).is_err());
    fs::write(dir.path().join("file"), b"").unwrap();
    assert!(Creation::begin(weekly(), &dir.path().join("file")).is_err());
    let plan = Plan::new(&request("09-03-2026", "09-03-2026", 6)).unwrap();
    let result = Creation::begin(plan, dir.path()).unwrap().step(false);
    assert!(result.done);
    assert_eq!(result.processed(), 0);
}
#[test]
fn concurrent_creators() {
    let dir = tempfile::tempdir().unwrap();
    let a = Creation::begin(weekly(), dir.path()).unwrap();
    let b = Creation::begin(weekly(), dir.path()).unwrap();
    let a = std::thread::spawn(move || {
        let mut a = a;
        a.step(false)
    });
    let b = std::thread::spawn(move || {
        let mut b = b;
        b.step(false)
    });
    let (a, b) = (a.join().unwrap(), b.join().unwrap());
    assert_eq!(a.created + b.created, 3);
    assert_eq!(a.existing + b.existing, 3);
    assert!(a.failure_reason.is_none() && b.failure_reason.is_none());
}
#[cfg(unix)]
#[test]
fn destination_rename_and_symlink_collisions() {
    use std::os::unix::fs::symlink;
    let dir = tempfile::tempdir().unwrap();
    let external = tempfile::tempdir().unwrap();
    for target in [external.path().to_owned(), external.path().join("missing")] {
        symlink(target, dir.path().join("09-03-2026")).unwrap();
        let result = Creation::begin(weekly(), dir.path()).unwrap().step(false);
        assert_eq!(result.processed(), 0);
        assert_eq!(result.error_code.as_deref(), Some("conflict"));
        fs::remove_file(dir.path().join("09-03-2026")).unwrap();
    }
    let old = dir.path().join("old");
    let moved = dir.path().join("moved");
    fs::create_dir(&old).unwrap();
    let mut operation = Creation::begin(weekly(), &old).unwrap();
    fs::rename(&old, &moved).unwrap();
    symlink(external.path(), &old).unwrap();
    assert_eq!(operation.step(false).created, 3);
    assert_eq!(fs::read_dir(&moved).unwrap().count(), 3);
    assert_eq!(fs::read_dir(external.path()).unwrap().count(), 0);
}

#[test]
fn independent_cancellation_signal_is_terminal() {
    use std::sync::atomic::Ordering;
    let dir = tempfile::tempdir().unwrap();
    let mut creation = Creation::begin(weekly(), dir.path()).unwrap();
    let cancellation = creation.cancellation();
    std::thread::spawn(move || cancellation.store(true, Ordering::Relaxed))
        .join()
        .unwrap();
    let result = creation.step(false);
    assert!(result.cancelled && result.done);
    assert_eq!(result.processed(), 0);
    assert_eq!(creation.step(false), result);
}
#[cfg(windows)]
#[test]
fn windows_directory_junction_is_a_conflict() {
    let dir = tempfile::tempdir().unwrap();
    let target = tempfile::tempdir().unwrap();
    let result = std::process::Command::new("cmd")
        .args(["/C", "mklink", "/J"])
        .arg(dir.path().join("09-03-2026"))
        .arg(target.path())
        .output()
        .unwrap();
    assert!(
        result.status.success(),
        "{}",
        String::from_utf8_lossy(&result.stderr)
    );
    let result = Creation::begin(weekly(), dir.path()).unwrap().step(false);
    assert_eq!(result.processed(), 0);
    assert_eq!(result.error_code.as_deref(), Some("conflict"));
    assert_eq!(std::fs::read_dir(target.path()).unwrap().count(), 0);
}

#[test]
fn localized_search_regressions() {
    for query in ["０９－１０", "٠٩-١٠", "०९-१०", "09\u{200b}-10"] {
        assert_eq!(weekly().preview(query, 200), vec!["09-10-2026"]);
    }
    for query in ["²", "①", "𝟚", "\u{200b}"] {
        assert!(weekly().preview(query, 200).is_empty(), "{query}");
    }
    assert_eq!(weekly().preview("\u{ad}2026", 200).len(), 3);
}
