use criterion::{criterion_group, criterion_main, Criterion};
use lumen::{common::logging, VERSION};

fn benchmark_version() -> String {
    VERSION.to_string()
}

fn benchmark_logging_init() {
    logging::init();
}

fn benchmark_timer() {
    let timer = logging::Timer::start("benchmark_operation");
    // Simulate some work
    std::thread::sleep(std::time::Duration::from_micros(1));
    timer.stop();
}

fn benchmark_error_creation() {
    use lumen::common::Error;
    let _error = Error::io("Test error message");
}

fn criterion_benchmark(c: &mut Criterion) {
    c.bench_function("version", |b| b.iter(benchmark_version));

    c.bench_function("logging_init", |b| b.iter(benchmark_logging_init));

    c.bench_function("timer", |b| b.iter(benchmark_timer));

    c.bench_function("error_creation", |b| b.iter(benchmark_error_creation));
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);
