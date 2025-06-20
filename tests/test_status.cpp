#include <gtest/gtest.h>
#include <lumen/common/status.h>

using namespace lumen;

TEST(StatusTest, SuccessStatus) {
    auto status = Status::ok();
    EXPECT_TRUE(status.is_ok());
    EXPECT_FALSE(status.is_error());
    EXPECT_TRUE(status);
    EXPECT_EQ(status.code(), ErrorCode::OK);
    EXPECT_EQ(status.message(), "");
}

TEST(StatusTest, ErrorStatus) {
    auto status = Status::error("test error");
    EXPECT_FALSE(status.is_ok());
    EXPECT_TRUE(status.is_error());
    EXPECT_FALSE(status);
    EXPECT_EQ(status.code(), ErrorCode::UNKNOWN);
    EXPECT_EQ(status.message(), "test error");
}

TEST(StatusTest, SpecificErrors) {
    auto invalid = Status::invalid_argument("bad input");
    EXPECT_EQ(invalid.code(), ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(invalid.message(), "bad input");

    auto not_found = Status::not_found("key missing");
    EXPECT_EQ(not_found.code(), ErrorCode::NOT_FOUND);
    EXPECT_EQ(not_found.message(), "key missing");

    auto corruption = Status::corruption("checksum failed");
    EXPECT_EQ(corruption.code(), ErrorCode::CORRUPTION);
    EXPECT_EQ(corruption.message(), "checksum failed");

    auto io_error = Status::io_error("disk read failed");
    EXPECT_EQ(io_error.code(), ErrorCode::IO_ERROR);
    EXPECT_EQ(io_error.message(), "disk read failed");
}

TEST(ResultTest, SuccessResult) {
    auto result = Result<int>::ok(42);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_error());
    EXPECT_TRUE(result);
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(result.value_or(0), 42);
}

TEST(ResultTest, ErrorResult) {
    auto result = Result<int>::error(ErrorCode::NOT_FOUND, "not found");
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_error());
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().code(), ErrorCode::NOT_FOUND);
    EXPECT_EQ(result.error().message(), "not found");
    EXPECT_EQ(result.value_or(99), 99);

    // value() should throw
    EXPECT_THROW(result.value(), std::runtime_error);
}

TEST(ResultTest, MoveSemantics) {
    auto result = Result<std::string>::ok("hello");
    std::string value = std::move(result).value();
    EXPECT_EQ(value, "hello");
}

TEST(ResultTest, AndThen) {
    auto double_if_positive = [](int x) -> Result<int> {
        if (x > 0) {
            return Result<int>::ok(x * 2);
        }
        return Result<int>::error(ErrorCode::INVALID_ARGUMENT, "not positive");
    };

    auto result1 = Result<int>::ok(5).and_then(double_if_positive);
    EXPECT_TRUE(result1.is_ok());
    EXPECT_EQ(result1.value(), 10);

    auto result2 = Result<int>::ok(-5).and_then(double_if_positive);
    EXPECT_FALSE(result2.is_ok());
    EXPECT_EQ(result2.error().code(), ErrorCode::INVALID_ARGUMENT);

    auto result3 = Result<int>::error(ErrorCode::IO_ERROR, "failed").and_then(double_if_positive);
    EXPECT_FALSE(result3.is_ok());
    EXPECT_EQ(result3.error().code(), ErrorCode::IO_ERROR);
}

TEST(ResultTest, VoidResult) {
    auto ok = Result<void>::ok();
    EXPECT_TRUE(ok.is_ok());
    EXPECT_FALSE(ok.is_error());

    auto error = Result<void>::error(ErrorCode::PERMISSION_DENIED, "access denied");
    EXPECT_FALSE(error.is_ok());
    EXPECT_TRUE(error.is_error());
    EXPECT_EQ(error.error().code(), ErrorCode::PERMISSION_DENIED);
}