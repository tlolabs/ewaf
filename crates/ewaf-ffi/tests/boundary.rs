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
    assert_eq!(ewaf_date_add(1, 0), 1);
    assert_eq!(ewaf_date_add(1, 1), 2);
    assert_eq!(ewaf_date_add(1, -1), 0);
    assert_eq!(ewaf_date_add(1, i32::MIN), 0);
    assert_eq!(ewaf_date_add(3_652_059, 1), 0);
    assert_eq!(ewaf_date_add(3_652_059, i32::MAX), 0);
    assert_eq!(ewaf_date_add(3_652_059, i32::MIN), 0);
    assert_eq!(ewaf_date_add(0, 1), 0);
    unsafe {
        assert_eq!(ewaf_date_parse(b"01-01-0001".as_ptr(), 10), 1);
        assert_eq!(ewaf_date_parse(std::ptr::null(), 10), 0);
        let mut name_buf = [0u8; 10];
        assert!(ewaf_date_name(1, name_buf.as_mut_ptr()));
        assert_eq!(&name_buf, b"01-01-0001");
        assert!(!ewaf_date_name(0, name_buf.as_mut_ptr()));
        assert!(!ewaf_date_name(1, std::ptr::null_mut()));
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

#[test]
fn exact_date_boundary_and_validation() {
    let res = call(json!({"op":"exact","start":"01-01-2024","end":"01-01-2024"}));
    assert_eq!(res["ok"], true);
    assert_eq!(res["value"]["start"], "01-01-2024");
    assert_eq!(res["value"]["end"], "01-01-2024");

    let res = call(json!({"op":"exact","start":"05-15-2024","end":"05-14-2024"}));
    assert_eq!(res["ok"], false);
    assert_eq!(res["error"]["code"], "reversed_range");

    let res = call(json!({"op":"exact","start":"01-01-2025","end":"12-31-2024"}));
    assert_eq!(res["ok"], false);
    assert_eq!(res["error"]["code"], "reversed_range");

    let res = call(json!({"op":"exact","start":"02-29-2024","end":"02-29-2024"}));
    assert_eq!(res["ok"], true);
    assert_eq!(res["value"]["start"], "02-29-2024");

    let res = call(json!({"op":"exact","start":"02-29-2023","end":"03-01-2023"}));
    assert_eq!(res["ok"], false);
    assert_eq!(res["error"]["code"], "invalid_date");

    let res = call(json!({"op":"exact","start":"13-01-2024","end":"12-01-2024"}));
    assert_eq!(res["ok"], false);
    assert_eq!(res["error"]["code"], "invalid_date");

    let res = call(json!({"op":"exact","start":"01-01-0001","end":"12-31-9999"}));
    assert_eq!(res["ok"], true);
    assert_eq!(res["value"]["start"], "01-01-0001");
    assert_eq!(res["value"]["end"], "12-31-9999");
}

#[test]
fn cancel_and_release_return_empty_object() {
    let cancel_res = call(json!({"op":"cancel","handle":12345}));
    assert_eq!(cancel_res["ok"], true);
    assert!(cancel_res["value"].as_object().unwrap().is_empty());

    let release_res = call(json!({"op":"release","handle":12345}));
    assert_eq!(release_res["ok"], true);
    assert!(release_res["value"].as_object().unwrap().is_empty());
}
