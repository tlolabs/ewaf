use ewaf_core::{Plan, PlanRequest};
use std::{hint::black_box, time::Instant};
fn main() {
    let request = PlanRequest {
        start: "01-01-0001".into(),
        end: "12-31-9999".into(),
        weekday: 5,
    };
    let start = Instant::now();
    for _ in 0..10 {
        let plan = Plan::new(black_box(&request)).unwrap();
        assert_eq!(plan.dates.len(), 521_723);
        black_box(plan.preview("9999", 200));
    }
    println!(
        "Full 9999-year plan + late-range search: {:?} per run (10 runs)",
        start.elapsed() / 10
    );
}
