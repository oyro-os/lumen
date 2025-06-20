#include <benchmark/benchmark.h>
#include <lumen/common/logging.h>
#include <lumen/common/status.h>
#include <lumen/lumen.h>
#include <random>
#include <vector>

using namespace lumen;

// Benchmark Status creation
static void BM_StatusCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto status = Status::ok();
        benchmark::DoNotOptimize(status);
    }
}
BENCHMARK(BM_StatusCreation);

// Benchmark Status error creation
static void BM_StatusErrorCreation(benchmark::State& state) {
    for (auto _ : state) {
        auto status = Status::error("Something went wrong");
        benchmark::DoNotOptimize(status);
    }
}
BENCHMARK(BM_StatusErrorCreation);

// Benchmark Result<int> success case
static void BM_ResultIntSuccess(benchmark::State& state) {
    for (auto _ : state) {
        auto result = Result<int>::ok(42);
        benchmark::DoNotOptimize(result);
        if (result.is_ok()) {
            benchmark::DoNotOptimize(result.value());
        }
    }
}
BENCHMARK(BM_ResultIntSuccess);

// Benchmark Result<std::string> with move
static void BM_ResultStringMove(benchmark::State& state) {
    for (auto _ : state) {
        auto result = Result<std::string>::ok("Hello, World!");
        benchmark::DoNotOptimize(result);
        if (result.is_ok()) {
            std::string value = std::move(result).value();
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_ResultStringMove);

// Benchmark logging (disabled)
static void BM_LoggingDisabled(benchmark::State& state) {
    // Set log level to OFF
    Logger::instance().set_level(LogLevel::OFF);

    for (auto _ : state) {
        LUMEN_LOG_INFO("This should not be logged");
    }
}
BENCHMARK(BM_LoggingDisabled);

// Benchmark logging overhead when enabled
static void BM_LoggingEnabled(benchmark::State& state) {
    // Redirect stderr to null
    auto old_buf = std::cerr.rdbuf();
    std::ostringstream null_stream;
    std::cerr.rdbuf(null_stream.rdbuf());

    Logger::instance().set_level(LogLevel::INFO);

    for (auto _ : state) {
        LUMEN_LOG_INFO("Benchmark message");
    }

    // Restore stderr
    std::cerr.rdbuf(old_buf);
}
BENCHMARK(BM_LoggingEnabled);

// Main function provided by Google Benchmark
BENCHMARK_MAIN();