// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
//! ABI v1: UTF-8 JSON requests, explicit response ownership, opaque integer handles.
use ewaf_core::{CivilDate, Creation, Error, Plan, PlanRequest, PREVIEW_LIMIT};
use serde::{Deserialize, Serialize};
use std::{
    collections::HashMap,
    ffi::{c_char, CString},
    path::Path,
    sync::{
        atomic::{AtomicBool, AtomicU64, Ordering},
        Arc, Mutex, OnceLock,
    },
};

#[derive(Serialize)]
struct OkEnvelope<T> {
    ok: bool,
    value: T,
}

#[derive(Serialize)]
struct ErrEnvelope<'a> {
    ok: bool,
    error: &'a Error,
}

#[derive(Serialize)]
struct InfoValue {
    abi: u32,
    version: &'static str,
}

#[derive(Serialize)]
struct ExactValue {
    start: String,
    end: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct PlanValue {
    count: usize,
    requires_confirmation: bool,
    dates: Vec<String>,
}

#[derive(Serialize)]
struct BeginValue {
    handle: u64,
}

#[derive(Serialize)]
struct EmptyValue {}

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
fn dispatch(request: Request) -> Result<String, Error> {
    match request {
        Request::Info {} => serde_json::to_string(&OkEnvelope {
            ok: true,
            value: InfoValue {
                abi: 1,
                version: env!("CARGO_PKG_VERSION"),
            },
        })
        .map_err(|_| internal()),
        Request::Exact { start, end } => {
            let start = CivilDate::parse(start.trim())?;
            let end = CivilDate::parse(end.trim())?;
            if start > end {
                return Err(Error::new(
                    "reversed_range",
                    "The end date must be on or after the start date.",
                ));
            }
            serde_json::to_string(&OkEnvelope {
                ok: true,
                value: ExactValue {
                    start: start.name(),
                    end: end.name(),
                },
            })
            .map_err(|_| internal())
        }
        Request::Plan {
            plan,
            search,
            limit,
            all,
        } => {
            let plan = Plan::new(&plan)?;
            let dates = if all {
                plan.dates.iter().map(|d| d.name()).collect()
            } else {
                plan.preview(&search, limit.min(PREVIEW_LIMIT))
            };
            serde_json::to_string(&OkEnvelope {
                ok: true,
                value: PlanValue {
                    count: plan.dates.len(),
                    requires_confirmation: plan.requires_confirmation(),
                    dates,
                },
            })
            .map_err(|_| internal())
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
            serde_json::to_string(&OkEnvelope {
                ok: true,
                value: BeginValue { handle },
            })
            .map_err(|_| internal())
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
            serde_json::to_string(&OkEnvelope {
                ok: true,
                value: result,
            })
            .map_err(|_| internal())
        }
        Request::Cancel { handle } => {
            if let Some(operation) = operations().lock().map_err(|_| internal())?.get(&handle) {
                operation.cancellation.store(true, Ordering::Relaxed);
            }
            serde_json::to_string(&OkEnvelope {
                ok: true,
                value: EmptyValue {},
            })
            .map_err(|_| internal())
        }
        Request::Release { handle } => {
            operations().lock().map_err(|_| internal())?.remove(&handle);
            serde_json::to_string(&OkEnvelope {
                ok: true,
                value: EmptyValue {},
            })
            .map_err(|_| internal())
        }
    }
}

pub fn request(input: &[u8]) -> String {
    let result = std::panic::catch_unwind(|| {
        let request: Request = serde_json::from_slice(input).map_err(|_| {
            Error::new(
                "invalid_request",
                "The application sent an invalid core request.",
            )
        })?;
        dispatch(request)
    });
    match result {
        Ok(Ok(json)) => json,
        Ok(Err(error)) => serde_json::to_string(&ErrEnvelope {
            ok: false,
            error: &error,
        })
        .unwrap_or_else(|_| {
            r#"{"ok":false,"error":{"code":"internal","message":"Serialization error"}}"#.into()
        }),
        Err(_) => serde_json::to_string(&ErrEnvelope {
            ok: false,
            error: &internal(),
        })
        .unwrap_or_else(|_| {
            r#"{"ok":false,"error":{"code":"internal","message":"Panic occurred"}}"#.into()
        }),
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
    let mut resp = request(bytes).into_bytes();
    resp.push(0);
    match CString::from_vec_with_nul(resp) {
        Ok(c) => c.into_raw(),
        Err(_) => CString::new(
            r#"{"ok":false,"error":{"code":"internal","message":"Interior NUL byte in response"}}"#,
        )
        .unwrap_or_else(|_| CString::new(r#"{"ok":false}"#).unwrap())
        .into_raw(),
    }
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
    let ascii = date.format_ascii();
    unsafe {
        std::ptr::copy_nonoverlapping(ascii.as_ptr(), output, 10);
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
