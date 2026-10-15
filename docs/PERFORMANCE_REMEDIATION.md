# EWAF Performance Remediation and Verification Report

This document records the independent assessment, methodological corrections, verified measurements, code remediations, and regression test coverage for the optimizations initially documented in `docs/PERFORMANCE.md`.

---

## 1. Executive Summary

An independent assessment of the "Autonomous Performance Audit and Optimization Report" (commits `d1fcfa3` and `46d25fb`) was performed against the current active repository state (HEAD `b6be03d` on branch `main`).

The audit verified that while the algorithmic and boundary optimizations introduced in `d1fcfa3` were genuinely beneficial, several reported measurements and implementation details suffered from methodological flaws or introduced behavioral defects:
1. **Startup Latency Measurement Flaw**: The original startup benchmark script executed an unconditional `time.sleep(0.5)`, artificially recording ~505 ms as application startup latency. When measured against observable system milestones, process launch via LaunchServices takes **~27 ms** and PID observation takes **~49 ms**.
2. **Microbenchmark Truncation Flaw**: The report claimed `CivilDate::adding(7)` took `<1.0 ns` (recorded as `0.000 ns`). This was an artifact of integer division in Rust's `Duration / N` truncating sub-microsecond totals to zero, combined with loop-invariant hoisting. When measured with varying inputs and float nanosecond precision, `CivilDate::adding(±7)` takes **1.09 ns**.
3. **FFI Python Harness Defect**: The previously supplied ctypes benchmark script declared `ewaf_request.restype = c_char_p` and then called `ewaf_string_free`, triggering memory corruption and double frees. The harness was remediated to use opaque `c_void_p`, copy string memory, and free exactly once.
4. **Swift Locale Freezing Defect**: In `platform/macos/Sources/EWAF/Views/RangeForm.swift`, `localGregorian` was cached as a `static let` bound to `Locale.current`. This permanently froze the startup locale and broke dynamic locale adaptation. This was corrected using `@Environment(\.locale)` and `RangeForm.calendar(for:)`.
5. **Residual FFI Heap Allocation**: In `crates/ewaf-ffi/src/lib.rs`, `Cancel` and `Release` operations allocated `serde_json::Map::<String, Value>::new()` on the heap. This was remediated with a zero-allocation `EmptyValue {}` struct.

---

## 2. System Environment and Baseline Configuration

- **Operating System**: macOS 14+ (Darwin 27.0.0, Apple Silicon arm64)
- **Toolchain**:
  - `rustc 1.98.1` / `cargo 1.98.1`
  - Apple Swift version 6.4 (swiftlang-6.4.0.128.29 clang-1800.0.8.2)
  - CMake 4.4.4
  - Python 3.15.0
- **Git State**:
  - Branch: `main`
  - Active HEAD: `b6be03d`
  - Preceding optimization commits: `d1fcfa3`, `46d25fb`
  - Intermediate security & lifecycle commits preserved: `2be96be` through `b6be03d`

---

## 3. Findings Ledger: Verification of 8 Reported Optimizations

| ID | Optimization Focus | Reported Claim | Independent Code Verification | Status | Correctness & Performance Implications | Action Taken & Verification |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **A** | Boundary-Only Validation for `Request::Exact` (`crates/ewaf-ffi`) | Eliminates 2MB heap allocation and drops latency from 1.332 ms to 2.833 µs (470x speedup) by avoiding `Plan::new(&plan)` for date order validation. | Lines 75–78 and 117–134 in `crates/ewaf-ffi/src/lib.rs` directly compare `start > end`. | **Verified & Correct** | Correctness preserved. Prevents 2.08 MB heap allocation during exact date text entry in both Mac and Qt presentations. | Added boundary tests in `crates/ewaf-ffi/tests/boundary.rs` (equal dates, reversed dates across years, leap day, invalid date formats, 9999-yr ranges). Verified passing. |
| **B** | Fast-Path Intra-Year Date Iteration (`crates/ewaf-core`) | `Plan::new` advances dates using `checked_add_days(7)` instead of `from_num_days_from_ce_opt`, speeding up 9999-yr plan by 51.1% (1.34 ms to 0.655 ms) and 1-yr plan by 47.1% (155 ns to 82 ns). | `Plan::new` in `crates/ewaf-core/src/lib.rs` uses `current.0.checked_add_days(chrono::Days::new(7))`. | **Verified & Correct** | Chrono's `checked_add_days` bypasses 400-year cycle floor divisions for ~51 of 52 weeks in a Gregorian year. | Added `range_stepping_and_cycle_consistency` in `behavior.rs`, verifying all 20,871 weekly steps across a complete 400-year cycle (1600–2000) match `from_ordinal` byte-for-byte. |
| **C** | Lookup-Table Decimal Formatting (`crates/ewaf-core`) | `CivilDate::format_ascii` uses a 200-byte precomputed table `DIGITS_LUT` for 2-digit numbers, speeding up `CivilDate::name` by 25%. | Lines 109–138 of `crates/ewaf-core/src/lib.rs` define `DIGITS_LUT` and index `m`, `d`, `y_hi`, `y_lo`. | **Verified & Correct** | Index bounds are mathematically guaranteed (`m <= 24`, `d <= 62`, `y_hi <= 198`, `y_lo <= 198` within the 200-byte table). Eliminates division instructions. | Added `exhaustive_lut_and_formatting` in `behavior.rs` validating formatting and parsing across all months, days, and leap/century rules. |
| **D** | Bounded Search & Early Exit (`crates/ewaf-core`) | `Plan::preview` rejects queries > 10 characters or containing non-date characters in ~15 ns rather than scanning all 521,723 dates. | Lines 224–232 of `crates/ewaf-core/src/lib.rs` check `search_bytes.len() > 10` and character set before scanning. | **Verified & Correct** | Preserves width-insensitive and localized Unicode search while preventing multi-millisecond scans on invalid text. | Re-measured invalid search latency (13.0–15.3 ns). Verified with localized search tests in `behavior.rs`. |
| **E** | Zero-Allocation Common Path in Folder Creation (`crates/ewaf-core`) | `Creation::step` formats date names onto stack via `[u8; 10]` and views them as `&str`, avoiding heap allocations in happy path. | Lines 364–375 of `crates/ewaf-core/src/lib.rs` use `date.format_ascii()` and `std::str::from_utf8_unchecked`. | **Verified with Qualification** | Userspace string heap allocation is avoided. However, the report's claim of "zero allocations in entire path" was overstated because OS kernel and filesystem syscalls perform internal buffers. | Re-benchmarked 25-folder creation across 10 fresh temporary directories (median 836.2 µs) vs existing directories (median 28.6 µs). |
| **F** | Strongly Typed FFI Response Serialization (`crates/ewaf-ffi`) | Replaces dynamic `serde_json::Value` trees with typed structs, cutting serialization latency in half. | Typed envelopes (`OkEnvelope`, `ExactValue`, `PlanValue`, `BeginValue`) are used, but `Cancel` and `Release` still allocated `Map::new()`. | **Remediated** | Heap allocation occurred on every cancellation and release call. | Replaced `serde_json::Map::<String, Value>::new()` with `EmptyValue {}` in `crates/ewaf-ffi/src/lib.rs`. Verified empty object serialization in `boundary.rs`. |
| **G** | Static Calendar Instance Caching (`platform/macos`) | `CivilDate.calendar` and `RangeForm.localGregorian` cached as `static let` to avoid repeated ICU `Calendar` allocations. | `CivilDate.calendar` cached with GMT/POSIX. However, `RangeForm.localGregorian` was defined as `static let ... value.locale = .current`, permanently freezing the startup locale. | **Remediated** | Broke dynamic locale adaptation if the user changed system language/region or in SwiftUI localized previews. | Refactored `RangeForm.swift` to derive calendar dynamically via `@Environment(\.locale)` and `RangeForm.calendar(for: locale)`. Added unit test in `WorkspaceTests.swift`. |
| **H** | Release Profile Optimization (`Cargo.toml`) | `strip = "debuginfo"` and explicit `opt-level = 3` reduced binary and archive size. | Root `Cargo.toml` contains `opt-level = 3` and `strip = "debuginfo"` under `[profile.release]`. | **Verified & Correct** | Removes DWARF debug sections from distributed release binaries while preserving external symbol table exports. | Verified binary builds and packaging sizes. |

---

## 4. Remediation of Measurement Methodologies

### 4.1 Startup Measurement
- **Prior Flaw**: The audit harness used an uncalibrated Python launch script containing `time.sleep(0.5)`, reporting ~505 ms as "startup latency".
- **Remediation**: Separated observable milestones:
  - **LaunchServices Process Dispatch (`open -n`)**: **26.7 ms** (median over 5 runs).
  - **Process PID Observable**: **48.9 ms** (median over 5 runs).
  - **Window Presentation & UI Readiness**: **~120 ms** (AppKit runloop entry, SwiftUI scene creation, and window server backing-store allocation).
- **Distinction**: Runs performed without rebooting or clearing OS buffer caches are warm launches. "Cold" launches require flushing filesystem caches.

### 4.2 Microbenchmark Precision and Barriers
- **Prior Flaw**: `CivilDate::adding(7)` was reported as `<1.0 ns` (and `0.000 ns` in stdout). This occurred because `date.adding(7)` with a constant `date` was loop-invariant, and standard integer duration division `total_duration / 100_000` truncated sub-microsecond totals to `0ns`.
- **Remediation**:
  - Implemented dynamic input alternation (`step = if (i & 1) == 0 { 7 } else { -7 }`) wrapped in `std::hint::black_box`.
  - Replaced integer division with floating-point nanoseconds: `elapsed.as_nanos() as f64 / N as f64`.
  - **Measured True Latency**: `CivilDate::adding(±7)` executes in **1.09 ns per call**.

### 4.3 FFI Python Harness Memory Management
- **Prior Flaw**: The benchmark script declared:
  ```python
  lib.ewaf_request.restype = ctypes.c_char_p
  ...
  lib.ewaf_string_free(res)
  ```
  `ctypes` automatically converts `c_char_p` into a Python `bytes` object upon return and discards the C pointer, making the subsequent `ewaf_string_free` call receive an invalid pointer, risking double-free corruption.
- **Remediation**:
  ```python
  lib.ewaf_request.restype = ctypes.c_void_p
  ptr = lib.ewaf_request(req_bytes, len(req_bytes))
  try:
      res_str = ctypes.cast(ptr, ctypes.c_char_p).value.decode("utf-8")
      return json.loads(res_str)
  finally:
      lib.ewaf_string_free(ptr)
  ```

### 4.4 Filesystem I/O Variance
- **Prior Flaw**: Single-run directory creation timings were reported without variance or isolation between fresh and existing directories.
- **Remediation**: Evaluated 10 runs across 10 clean temporary directories:
  - **Fresh 25-folder creation**: Median **836.2 µs**, Mean **829.3 µs**, StdDev **58.0 µs**.
  - **Existing 25-folder stat**: Median **28.6 µs**, Mean **29.3 µs**, StdDev **2.4 µs**.

---

## 5. Code Remediations Implemented

### 5.1 FFI Empty Value Optimization (`crates/ewaf-ffi`)
In `crates/ewaf-ffi/src/lib.rs`:
- Defined `EmptyValue` serializing directly to `{}` without dynamic heap structures:
  ```rust
  #[derive(Serialize)]
  struct EmptyValue {}
  ```
- Replaced `value: serde_json::Map::<String, Value>::new()` in `Request::Cancel` and `Request::Release` with `value: EmptyValue {}`.
- Removed unused `serde_json::Value` import from `crates/ewaf-ffi/src/lib.rs`.

### 5.2 Swift Locale Dynamic Adaptation (`platform/macos`)
In `platform/macos/Sources/EWAF/Views/RangeForm.swift`:
- Replaced the static, launch-frozen `localGregorian` calendar with an environment-aware helper:
  ```swift
  @Environment(\.locale) private var locale

  static func calendar(for locale: Locale) -> Calendar {
      var value = CivilDate.calendar
      value.locale = locale
      return value
  }

  private var localGregorian: Calendar {
      Self.calendar(for: locale)
  }
  ```
- Preserved the static `CivilDate.calendar` ISO8601 GMT base template while allowing DatePickers to adapt dynamically to runtime locale changes.

---

## 6. Regression Test Coverage

1. **Rust Core (`crates/ewaf-core/tests/behavior.rs`)**:
   - `exhaustive_lut_and_formatting`: Validates `DIGITS_LUT` and date formatting/parsing across all months, days, leap years (e.g. 2000, 2024), non-leap centuries (1900, 2100), and boundary years (0001, 9999).
   - `range_stepping_and_cycle_consistency`: Tests all 20,871 weekly increments across a full 400-year cycle (1600–2000), verifying exact equivalence between `adding(7)` and `from_ordinal`.
2. **Rust FFI (`crates/ewaf-ffi/tests/boundary.rs`)**:
   - `exact_date_boundary_and_validation`: Tests `Request::Exact` with equal start/end dates, reversed dates within same month, reversed dates across years, leap day boundaries, invalid dates, and 9999-year ranges.
   - `cancel_and_release_return_empty_object`: Verifies `Cancel` and `Release` return `{"ok":true,"value":{}}`.
3. **Swift App (`tests/macos/EWAFAppTests/WorkspaceTests.swift`)**:
   - `testRangeFormCalendarLocaleDynamism`: Verifies that `RangeForm.calendar(for:)` retains the ISO8601 calendar identifier and GMT time zone while adopting distinct locales (`en_US`, `fr_FR`, `ar_EG`, `ja_JP`).

---

## 7. Verification Results

All automated suites passed locally with zero failures:
- `cargo test --workspace`: **44 passed, 0 failed, 1 ignored** (Sparkle signing check intended for packaging CI).
- `cargo clippy --workspace --all-targets`: **0 warnings**.
- `cargo fmt --check`: **Clean, zero formatting diffs**.
- `swift test`: **40 native tests passed (25 EWAFCoreTests, 15 EWAFAppTests), 0 failures**.
- `./script/test_qt.sh`: **21 shared presentation/FFI/headless scenarios passed, 0 failures**.
- `python3 -m unittest discover tests/release`: **34 release tests passed, 0 failures**.

All commits and changes remain strictly local. No remote git operations (push, tags, PRs) were performed.
