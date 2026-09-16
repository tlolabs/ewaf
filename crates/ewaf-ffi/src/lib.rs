//! ABI v1: UTF-8 JSON requests, explicit response ownership, opaque integer handles.
use ewaf_core::{CivilDate, Creation, Error, Plan, PlanRequest, PREVIEW_LIMIT};
use serde::Deserialize;
use serde_json::{json, Value};
use std::{
    collections::HashMap,
    ffi::{c_char, CString},
    path::Path,
    sync::{
        atomic::{AtomicBool, AtomicU64, Ordering},
        Arc, Mutex, OnceLock,
    },
};

struct Operation {
    creation: Mutex<Creation>,
    cancellation: Arc<AtomicBool>,
}
type Operations = Mutex<HashMap<u64, Arc<Operation>>>;
static OPERATIONS: OnceLock<Operations> = OnceLock::new();
static NEXT: AtomicU64 = AtomicU64::new(1);
fn operations() -> &'static Operations {
    OPERATIONS.get_or_init(Mutex::default)
}
fn internal() -> Error {
    Error::new(
        "internal",
        "An internal error occurred. Close this window and retry.",
    )
}

#[derive(Deserialize)]
#[serde(tag = "op", rename_all = "snake_case", deny_unknown_fields)]
enum Request {
    Info {},
    Exact {
        start: String,
        end: String,
    },
    Plan {
        plan: PlanRequest,
        #[serde(default)]
        search: String,
        #[serde(default = "preview_limit")]
        limit: usize,
        #[serde(default)]
        all: bool,
    },
    Begin {
        plan: PlanRequest,
        destination: String,
    },
    Step {
        handle: u64,
        #[serde(default)]
        cancelled: bool,
    },
    Cancel {
        handle: u64,
    },
    Release {
        handle: u64,
    },
}
fn preview_limit() -> usize {
    PREVIEW_LIMIT
}
fn dispatch(request: Request) -> Result<Value, Error> {
    match request {
        Request::Info {} => Ok(json!({"abi":1,"version":env!("CARGO_PKG_VERSION")})),
        Request::Exact { start, end } => {
            let start = CivilDate::parse(start.trim())?;
            let end = CivilDate::parse(end.trim())?;
            let plan = PlanRequest {
                start: start.name(),
                end: end.name(),
                weekday: 1,
            };
            Plan::new(&plan)?;
            Ok(json!({"start":start.name(),"end":end.name()}))
        }
        Request::Plan {
            plan,
            search,
            limit,
            all,
        } => {
            let plan = Plan::new(&plan)?;
            Ok(
                json!({"count":plan.dates.len(),"requiresConfirmation":plan.requires_confirmation(),"dates":if all { plan.dates.iter().map(|d| d.name()).collect() } else { plan.preview(&search, limit.min(PREVIEW_LIMIT)) }}),
            )
        }
        Request::Begin { plan, destination } => {
            let creation = Creation::begin(Plan::new(&plan)?, Path::new(&destination))?;
            let handle = NEXT.fetch_add(1, Ordering::Relaxed);
            operations().lock().map_err(|_| internal())?.insert(
                handle,
                Arc::new(Operation {
                    cancellation: creation.cancellation(),
                    creation: Mutex::new(creation),
                }),
            );
            Ok(json!({"handle":handle}))
        }
        Request::Step { handle, cancelled } => {
            let creation = operations()
                .lock()
                .map_err(|_| internal())?
                .get(&handle)
                .cloned()
                .ok_or_else(|| {
                    Error::new(
                        "invalid_handle",
                        "The operation is no longer available. Retry the range.",
                    )
                })?;
            let result = creation
                .creation
                .lock()
                .map_err(|_| internal())?
                .step(cancelled);
            Ok(serde_json::to_value(result).map_err(|_| internal())?)
        }
        Request::Cancel { handle } => {
            if let Some(operation) = operations().lock().map_err(|_| internal())?.get(&handle) {
                operation.cancellation.store(true, Ordering::Relaxed);
            }
            Ok(json!({}))
        }
        Request::Release { handle } => {
            operations().lock().map_err(|_| internal())?.remove(&handle);
            Ok(json!({}))
        }
    }
}

pub fn request(input: &[u8]) -> String {
    let response = std::panic::catch_unwind(|| {
        let request: Request = serde_json::from_slice(input).map_err(|_| {
            Error::new(
                "invalid_request",
                "The application sent an invalid core request.",
            )
        })?;
        dispatch(request)
    })
    .unwrap_or_else(|_| Err(internal()));
    match response {
        Ok(value) => json!({"ok":true,"value":value}).to_string(),
        Err(error) => json!({"ok":false,"error":error}).to_string(),
    }
}

/// # Safety
/// `input` must reference `len` readable bytes for this call, or be null with len=0.
/// Free the returned allocation exactly once with ewaf_string_free.
#[no_mangle]
pub unsafe extern "C" fn ewaf_request(input: *const u8, len: usize) -> *mut c_char {
    let bytes = if input.is_null() || len > 16 * 1024 * 1024 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(input, len) }
    };
    CString::new(request(bytes))
        .expect("JSON contains no raw NUL")
        .into_raw()
}
/// # Safety
/// `value` is null or a live allocation returned by ewaf_request; do not free twice.
#[no_mangle]
pub unsafe extern "C" fn ewaf_string_free(value: *mut c_char) {
    if !value.is_null() {
        drop(unsafe { CString::from_raw(value) });
    }
}
#[no_mangle]
pub extern "C" fn ewaf_date_ordinal(year: i32, month: u32, day: u32) -> i32 {
    CivilDate::new(year, month, day)
        .map(|d| d.ordinal())
        .unwrap_or(0)
}
#[no_mangle]
pub extern "C" fn ewaf_date_component(ordinal: i32, component: u32) -> i32 {
    CivilDate::from_ordinal(ordinal)
        .map(|d| match component {
            0 => d.year(),
            1 => d.month() as i32,
            2 => d.day() as i32,
            3 => d.weekday() as i32,
            _ => 0,
        })
        .unwrap_or(0)
}
/// # Safety
/// `input` must reference len readable bytes, or be null.
#[no_mangle]
pub unsafe extern "C" fn ewaf_date_parse(input: *const u8, len: usize) -> i32 {
    if input.is_null() || len != 10 {
        return 0;
    }
    std::str::from_utf8(unsafe { std::slice::from_raw_parts(input, len) })
        .ok()
        .and_then(|s| CivilDate::parse(s).ok())
        .map(|d| d.ordinal())
        .unwrap_or(0)
}

/// # Safety
/// output must point to at least ten writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ewaf_date_name(ordinal: i32, output: *mut u8) -> bool {
    let Ok(date) = CivilDate::from_ordinal(ordinal) else {
        return false;
    };
    if output.is_null() {
        return false;
    }
    unsafe {
        std::ptr::copy_nonoverlapping(date.name().as_ptr(), output, 10);
    }
    true
}

#[no_mangle]
pub extern "C" fn ewaf_date_add(ordinal: i32, days: i32) -> i32 {
    CivilDate::from_ordinal(ordinal)
        .ok()
        .and_then(|d| d.adding(days))
        .map(|d| d.ordinal())
        .unwrap_or(0)
}
