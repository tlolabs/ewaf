use ewaf_ffi::*;
use serde_json::{json, Value};
fn call(v: Value) -> Value {
    serde_json::from_str(&request(v.to_string().as_bytes())).unwrap()
}
#[test]
fn envelope_exact_and_invalid_inputs() {
    assert_eq!(call(json!({"op":"info"}))["value"]["abi"], 1);
    for value in [
        b"garbage".as_slice(),
        b"{",
        b"\xff",
        br#"{"op":"missing"}"#,
        br#"{"op":"info","extra":0}"#,
    ] {
        assert_eq!(
            serde_json::from_str::<Value>(&request(value)).unwrap()["ok"],
            false
        );
    }
    assert_eq!(
        call(json!({"op":"exact","start":" 02-29-2024 ","end":"03-01-2024"}))["value"]["start"],
        "02-29-2024"
    );
    assert_eq!(
        call(json!({"op":"exact","start":"03-01-2024","end":"02-29-2024"}))["error"]["code"],
        "reversed_range"
    );
    assert_eq!(
        call(json!({"op":"step","handle":999}))["error"]["code"],
        "invalid_handle"
    );
    assert_eq!(call(json!({"op":"release","handle":999}))["ok"], true);
}
#[test]
fn c_allocation_and_date_boundary() {
    for _ in 0..100 {
        let bytes = br#"{"op":"info"}"#;
        unsafe {
            let response = ewaf_request(bytes.as_ptr(), bytes.len());
            assert!(!response.is_null());
            let text = std::ffi::CStr::from_ptr(response).to_str().unwrap();
            assert_eq!(serde_json::from_str::<Value>(text).unwrap()["ok"], true);
            ewaf_string_free(response);
            ewaf_string_free(std::ptr::null_mut());
        }
    }
    assert_eq!(ewaf_date_ordinal(1, 1, 1), 1);
    assert_eq!(ewaf_date_ordinal(2025, 2, 29), 0);
    assert_eq!(ewaf_date_component(1, 3), 2);
    assert_eq!(ewaf_date_component(0, 0), 0);
    unsafe {
        assert_eq!(ewaf_date_parse(b"01-01-0001".as_ptr(), 10), 1);
        assert_eq!(ewaf_date_parse(std::ptr::null(), 10), 0);
    }
}

#[test]
fn handles_creation_cancellation_and_release() {
    let dir = tempfile::tempdir().unwrap();
    let value = call(
        json!({"op":"begin","plan":{"start":"01-01-2020","end":"12-31-2030","weekday":5},"destination":dir.path().to_str().unwrap()}),
    );
    let handle = value["value"]["handle"].as_u64().unwrap();
    assert_eq!(
        call(json!({"op":"step","handle":handle}))["value"]["created"],
        25
    );
    std::thread::spawn(move || {
        assert_eq!(call(json!({"op":"cancel","handle":handle}))["ok"], true)
    })
    .join()
    .unwrap();
    let result = call(json!({"op":"step","handle":handle}));
    assert_eq!(result["value"]["cancelled"], true);
    assert_eq!(result["value"]["created"], 25);
    assert_eq!(call(json!({"op":"release","handle":handle}))["ok"], true);
    assert_eq!(call(json!({"op":"release","handle":handle}))["ok"], true);
    assert_eq!(
        call(json!({"op":"step","handle":handle}))["error"]["code"],
        "invalid_handle"
    );
}
