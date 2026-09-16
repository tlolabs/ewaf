//! Authoritative EWAF behavior. No presentation state or platform UI lives here.
use cap_std::{ambient_authority, fs::Dir};
use chrono::{Datelike, NaiveDate};
use serde::{Deserialize, Serialize};
use std::{
    io,
    path::Path,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
};
use unicode_general_category::{get_general_category, GeneralCategory};
use unicode_normalization::{char::is_combining_mark, UnicodeNormalization};

pub const PREVIEW_LIMIT: usize = 200;
pub const CONFIRM_THRESHOLD: usize = 250;
pub const PROGRESS_BATCH: usize = 25;

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Error {
    pub code: String,
    pub message: String,
}
impl Error {
    pub fn new(code: &str, message: &str) -> Self {
        Self {
            code: code.into(),
            message: message.into(),
        }
    }
    pub fn invalid_date() -> Self {
        Self::new(
            "invalid_date",
            "Choose a valid date between the years 1 and 9999.",
        )
    }
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.message)
    }
}
impl std::error::Error for Error {}

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct CivilDate(NaiveDate);
impl CivilDate {
    pub fn new(year: i32, month: u32, day: u32) -> Result<Self, Error> {
        if !(1..=9999).contains(&year) {
            return Err(Error::invalid_date());
        }
        NaiveDate::from_ymd_opt(year, month, day)
            .map(Self)
            .ok_or_else(Error::invalid_date)
    }
    pub fn parse(value: &str) -> Result<Self, Error> {
        let b = value.as_bytes();
        if b.len() != 10
            || b[2] != b'-'
            || b[5] != b'-'
            || !b
                .iter()
                .enumerate()
                .all(|(i, c)| i == 2 || i == 5 || c.is_ascii_digit())
        {
            return Err(Error::invalid_date());
        }
        Self::new(
            value[6..10].parse().map_err(|_| Error::invalid_date())?,
            value[0..2].parse().map_err(|_| Error::invalid_date())?,
            value[3..5].parse().map_err(|_| Error::invalid_date())?,
        )
    }
    pub fn from_ordinal(ordinal: i32) -> Result<Self, Error> {
        let date = NaiveDate::from_num_days_from_ce_opt(ordinal).ok_or_else(Error::invalid_date)?;
        Self::new(date.year(), date.month(), date.day())
    }
    pub fn ordinal(self) -> i32 {
        self.0.num_days_from_ce()
    }
    pub fn year(self) -> i32 {
        self.0.year()
    }
    pub fn month(self) -> u32 {
        self.0.month()
    }
    pub fn day(self) -> u32 {
        self.0.day()
    }
    /// Persisted compatibility numbering: Sunday=1, Monday=2, … Saturday=7.
    pub fn weekday(self) -> u32 {
        self.0.weekday().number_from_sunday()
    }
    pub fn name(self) -> String {
        format!("{:02}-{:02}-{:04}", self.month(), self.day(), self.year())
    }
    pub fn adding(self, days: i32) -> Option<Self> {
        self.ordinal()
            .checked_add(days)
            .and_then(|n| Self::from_ordinal(n).ok())
    }
}

#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
#[serde(deny_unknown_fields)]
pub struct PlanRequest {
    pub start: String,
    pub end: String,
    pub weekday: u32,
}
#[derive(Debug, Clone)]
pub struct Plan {
    pub dates: Vec<CivilDate>,
}
impl Plan {
    pub fn new(request: &PlanRequest) -> Result<Self, Error> {
        let start = CivilDate::parse(&request.start)?;
        let end = CivilDate::parse(&request.end)?;
        if start > end {
            return Err(Error::new(
                "reversed_range",
                "The end date must be on or after the start date.",
            ));
        }
        if !(1..=7).contains(&request.weekday) {
            return Err(Error::new(
                "invalid_weekday",
                "Choose a weekday from Sunday (1) through Saturday (7).",
            ));
        }
        let mut dates = Vec::new();
        let mut next = start.adding(((request.weekday + 7 - start.weekday()) % 7) as i32);
        while let Some(date) = next {
            if date > end {
                break;
            }
            dates.push(date);
            next = date.adding(7);
        }
        Ok(Self { dates })
    }
    pub fn requires_confirmation(&self) -> bool {
        self.dates.len() > CONFIRM_THRESHOLD
    }
    pub fn preview(&self, search: &str, limit: usize) -> Vec<String> {
        // Foundation's width-insensitive search is useful for full-width keyboard input.
        let search = normalize_search(search);
        self.dates
            .iter()
            .map(|d| d.name())
            .filter(|name| name.contains(&search))
            .take(limit)
            .collect()
    }
}

/// Date names contain only digits and hyphens. Match Foundation's useful
/// localized-search behavior without treating superscript/circled numbers as digits.
fn normalize_search(value: &str) -> String {
    value
        .nfd()
        .filter_map(|c| {
            if is_combining_mark(c) || get_general_category(c) == GeneralCategory::Format {
                return None;
            }
            if get_general_category(c) == GeneralCategory::DecimalNumber {
                // Unicode Nd sets are consecutive zero-through-nine runs (some adjacent).
                let mut first = c as u32;
                while first > 0
                    && char::from_u32(first - 1)
                        .is_some_and(|p| get_general_category(p) == GeneralCategory::DecimalNumber)
                {
                    first -= 1;
                }
                return char::from_u32(u32::from(b'0') + ((c as u32 - first) % 10));
            }
            if ('！'..='～').contains(&c) {
                return char::from_u32(c as u32 - 0xfee0);
            }
            Some(c)
        })
        .collect()
}

#[derive(Debug, Default, Clone, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub struct CreationResult {
    pub created: usize,
    pub existing: usize,
    pub cancelled: bool,
    pub failed_name: Option<String>,
    pub failure_reason: Option<String>,
    pub error_code: Option<String>,
    pub done: bool,
}
impl CreationResult {
    pub fn processed(&self) -> usize {
        self.created + self.existing
    }
}

/// Owns the open destination for the entire operation, even if its path is renamed.
/// Each step is bounded. Native executors run steps off the UI thread and may cancel
/// between steps; committed directories are preserved and retries are idempotent.
pub struct Creation {
    cancellation: Arc<AtomicBool>,
    destination: Dir,
    plan: Plan,
    result: CreationResult,
}
impl Creation {
    pub fn begin(plan: Plan, destination: &Path) -> Result<Self, Error> {
        let destination =
            Dir::open_ambient_dir(destination, ambient_authority()).map_err(|_| {
                Error::new(
                    "destination_unavailable",
                    "The destination is unavailable. Choose an existing folder you can write to.",
                )
            })?;
        Ok(Self {
            cancellation: Arc::new(AtomicBool::new(false)),
            destination,
            plan,
            result: CreationResult::default(),
        })
    }
    pub fn cancellation(&self) -> Arc<AtomicBool> {
        self.cancellation.clone()
    }
    pub fn step(&mut self, cancelled: bool) -> CreationResult {
        if self.result.done {
            return self.result.clone();
        }
        if cancelled {
            self.cancellation.store(true, Ordering::Relaxed);
        }
        if self.cancellation.load(Ordering::Relaxed) {
            self.result.cancelled = true;
            self.result.done = true;
            return self.result.clone();
        }
        for _ in 0..PROGRESS_BATCH {
            if self.cancellation.load(Ordering::Relaxed) {
                self.result.cancelled = true;
                self.result.done = true;
                break;
            }
            let Some(date) = self.plan.dates.get(self.result.processed()) else {
                break;
            };
            let name = date.name();
            match self.destination.create_dir(&name) {
                Ok(()) => self.result.created += 1,
                Err(e)
                    if e.kind() == io::ErrorKind::AlreadyExists
                        && self
                            .destination
                            .symlink_metadata(&name)
                            .is_ok_and(|m| m.is_dir() && !m.file_type().is_symlink()) =>
                {
                    self.result.existing += 1
                }
                Err(e) => {
                    let (code, reason) = match e.kind() {
                        io::ErrorKind::AlreadyExists => ("conflict", "a file or symbolic link already uses this name. Move it or choose another destination, then retry."),
                        io::ErrorKind::PermissionDenied | io::ErrorKind::ReadOnlyFilesystem => ("permission_denied", "the destination does not allow changes. Check its permissions or choose another folder."),
                        io::ErrorKind::StorageFull | io::ErrorKind::QuotaExceeded => ("storage_full", "the drive is full. Free some space, then retry."),
                        _ => ("io_failure", "the folder could not be created. Check that the drive is connected and writable, then retry."),
                    };
                    self.result.failure_reason = Some(format!("Stopped at {name}: {reason}"));
                    self.result.failed_name = Some(name);
                    self.result.error_code = Some(code.into());
                    self.result.done = true;
                    break;
                }
            }
        }
        if self.result.processed() == self.plan.dates.len() {
            self.result.done = true;
        }
        self.result.clone()
    }
}
