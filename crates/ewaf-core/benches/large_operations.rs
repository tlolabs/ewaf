use ewaf_core::{Plan, PlanRequest};
use std::{hint::black_box, time::Instant};
fn main() {
    let request = PlanRequest {
        start: "01-01-0001".into(),
        end: "12-31-9999".into(),
        weekday: 5,
    };
    let mut plan_total = std::time::Duration::ZERO;
    let mut preview_total = std::time::Duration::ZERO;
    let start = Instant::now();
    for _ in 0..10 {
        let p_start = Instant::now();
        let plan = Plan::new(black_box(&request)).unwrap();
        plan_total += p_start.elapsed();
        assert_eq!(plan.dates.len(), 521_723);
        let prev_start = Instant::now();
        black_box(plan.preview("9999", 200));
        preview_total += prev_start.elapsed();
    }
    println!(
        "Full 9999-year plan + late-range search: {:?} per run (10 runs)",
        start.elapsed() / 10
    );
    println!("  -> Plan::new (9999 yr): {:?} per run", plan_total / 10);
    println!(
        "  -> Plan::preview(\"9999\"): {:?} per run",
        preview_total / 10
    );

    // Baseline micro-benchmarks
    let plan = Plan::new(&request).unwrap();

    // 1. Preview with empty search (first 200)
    let t0 = Instant::now();
    for _ in 0..100 {
        black_box(plan.preview(black_box(""), 200));
    }
    println!(
        "  -> Plan::preview(\"\", 200): {:?} per run (100 runs)",
        t0.elapsed() / 100
    );

    // 2. Preview with early match (first 200)
    let t0 = Instant::now();
    for _ in 0..100 {
        black_box(plan.preview(black_box("01-04-0001"), 200));
    }
    println!(
        "  -> Plan::preview(\"01-04-0001\", 200): {:?} per run (100 runs)",
        t0.elapsed() / 100
    );

    // 3. 1-year plan
    let req_1yr = PlanRequest {
        start: "01-01-2024".into(),
        end: "12-31-2024".into(),
        weekday: 5,
    };
    let t0 = Instant::now();
    for _ in 0..10_000 {
        black_box(Plan::new(black_box(&req_1yr)).unwrap());
    }
    let elapsed = t0.elapsed();
    println!(
        "  -> Plan::new (1 yr / 52 dates): {:.2} ns per run (10k runs)",
        elapsed.as_nanos() as f64 / 10_000.0
    );

    // 4. Date formatting
    let date = plan.dates[0];
    let t0 = Instant::now();
    for _ in 0..100_000 {
        black_box(date.name());
    }
    let elapsed = t0.elapsed();
    println!(
        "  -> CivilDate::name: {:.2} ns per call (100k runs)",
        elapsed.as_nanos() as f64 / 100_000.0
    );

    // 5. Date parsing
    let t0 = Instant::now();
    for _ in 0..100_000 {
        black_box(ewaf_core::CivilDate::parse(black_box("09-17-2026")).unwrap());
    }
    let elapsed = t0.elapsed();
    println!(
        "  -> CivilDate::parse: {:.2} ns per call (100k runs)",
        elapsed.as_nanos() as f64 / 100_000.0
    );

    // 6. Date adding (varying inputs to prevent loop hoisting)
    let mut step_date = date;
    let t0 = Instant::now();
    for i in 0..100_000 {
        let step = black_box(if (i & 1) == 0 { 7 } else { -7 });
        step_date = black_box(step_date.adding(step).unwrap());
    }
    let elapsed = t0.elapsed();
    println!(
        "  -> CivilDate::adding(±7): {:.2} ns per call (100k runs)",
        elapsed.as_nanos() as f64 / 100_000.0
    );

    // 7. Preview with invalid/nonsense query
    let t0 = Instant::now();
    for _ in 0..10_000 {
        black_box(plan.preview(black_box("nonsense"), 200));
    }
    let elapsed = t0.elapsed();
    println!(
        "  -> Plan::preview(\"nonsense\", 200): {:.2} ns per call (10k runs)",
        elapsed.as_nanos() as f64 / 10_000.0
    );

    // 8. Preview with query longer than 10 chars
    let t0 = Instant::now();
    for _ in 0..10_000 {
        black_box(plan.preview(black_box("12345678901"), 200));
    }
    let elapsed = t0.elapsed();
    println!(
        "  -> Plan::preview(\"12345678901\", 200): {:.2} ns per call (10k runs)",
        elapsed.as_nanos() as f64 / 10_000.0
    );

    // 9. Preview mid-range match
    let t0 = Instant::now();
    for _ in 0..100 {
        black_box(plan.preview(black_box("06-15"), 200));
    }
    println!(
        "  -> Plan::preview(\"06-15\", 200): {:?} per run (100 runs)",
        t0.elapsed() / 100
    );

    // 10. Creation step (filesystem I/O and idempotent check)
    let req_create = PlanRequest {
        start: "01-01-2024".into(),
        end: "06-30-2024".into(),
        weekday: 5,
    };
    let plan_create = Plan::new(&req_create).unwrap();

    // 10 runs across fresh temporary directories
    let mut fresh_durations = Vec::new();
    for _ in 0..10 {
        let tmp = tempfile::tempdir().unwrap();
        let mut creation = ewaf_core::Creation::begin(plan_create.clone(), tmp.path()).unwrap();
        let t0 = Instant::now();
        let res = creation.step(false);
        assert_eq!(res.created, 25);
        fresh_durations.push(t0.elapsed());
    }
    fresh_durations.sort();
    let median_fresh = fresh_durations[fresh_durations.len() / 2];
    println!(
        "  -> Creation::step (25 new dirs, 10 fresh tempdirs): median = {:?}",
        median_fresh
    );

    let tmp = tempfile::tempdir().unwrap();
    let mut creation = ewaf_core::Creation::begin(plan_create.clone(), tmp.path()).unwrap();
    creation.step(false);

    let mut existing_durations = Vec::new();
    for _ in 0..10 {
        let mut idempotent = ewaf_core::Creation::begin(plan_create.clone(), tmp.path()).unwrap();
        let t0 = Instant::now();
        let res = idempotent.step(false);
        assert_eq!(res.existing, 25);
        existing_durations.push(t0.elapsed());
    }
    existing_durations.sort();
    let median_existing = existing_durations[existing_durations.len() / 2];
    println!(
        "  -> Creation::step (25 existing dirs, 10 runs): median = {:?}",
        median_existing
    );
}
