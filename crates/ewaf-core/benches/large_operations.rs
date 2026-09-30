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
    println!(
        "  -> Plan::new (1 yr / 52 dates): {:?} per run (10k runs)",
        t0.elapsed() / 10_000
    );

    // 4. Date formatting
    let date = plan.dates[0];
    let t0 = Instant::now();
    for _ in 0..100_000 {
        black_box(date.name());
    }
    println!(
        "  -> CivilDate::name: {:?} per call (100k runs)",
        t0.elapsed() / 100_000
    );

    // 5. Date parsing
    let t0 = Instant::now();
    for _ in 0..100_000 {
        black_box(ewaf_core::CivilDate::parse(black_box("09-17-2026")).unwrap());
    }
    println!(
        "  -> CivilDate::parse: {:?} per call (100k runs)",
        t0.elapsed() / 100_000
    );
}
