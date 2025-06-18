#include <gtest/gtest.h>
#include <lumen/types.h>

using namespace lumen;

class TypesTest : public ::testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TypesTest, ValueConstruction) {
    // Test null value
    Value null_val;
    EXPECT_TRUE(null_val.isNull());
    EXPECT_EQ(DataType::Null, null_val.type());

    // Test boolean
    Value bool_val(true);
    EXPECT_TRUE(bool_val.isBool());
    EXPECT_TRUE(bool_val.asBool());
    EXPECT_EQ(DataType::Boolean, bool_val.type());

    // Test integers
    Value int32_val(int32_t(42));
    EXPECT_TRUE(int32_val.isInt());
    EXPECT_EQ(42, int32_val.asInt());
    EXPECT_EQ(DataType::Int32, int32_val.type());

    Value int64_val(int64_t(1234567890L));
    EXPECT_TRUE(int64_val.isInt());
    EXPECT_EQ(1234567890L, int64_val.asInt());
    EXPECT_EQ(DataType::Int64, int64_val.type());

    // Test unsigned integers
    Value uint32_val(uint32_t(42));
    EXPECT_TRUE(uint32_val.isUInt());
    EXPECT_EQ(42u, uint32_val.asUInt());
    EXPECT_EQ(DataType::UInt32, uint32_val.type());

    // Test floating point
    Value float_val(3.14f);
    EXPECT_TRUE(float_val.isFloat());
    EXPECT_FLOAT_EQ(3.14f, static_cast<float>(float_val.asFloat()));
    EXPECT_EQ(DataType::Float32, float_val.type());

    Value double_val(3.14159);
    EXPECT_TRUE(double_val.isFloat());
    EXPECT_DOUBLE_EQ(3.14159, double_val.asFloat());
    EXPECT_EQ(DataType::Float64, double_val.type());

    // Test string
    Value str_val("Hello, Lumen!");
    EXPECT_TRUE(str_val.isString());
    EXPECT_EQ("Hello, Lumen!", str_val.asString());
    EXPECT_EQ(DataType::String, str_val.type());

    // Test blob
    std::vector<byte> blob = {0x01, 0x02, 0x03, 0x04};
    Value blob_val(blob);
    EXPECT_TRUE(blob_val.isBlob());
    EXPECT_EQ(blob, blob_val.asBlob());
    EXPECT_EQ(DataType::Blob, blob_val.type());

    // Test vector
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};
    Value vec_val(vec);
    EXPECT_TRUE(vec_val.isVector());
    EXPECT_EQ(vec, vec_val.asVector());
    EXPECT_EQ(DataType::Vector, vec_val.type());

    // Test timestamp
    Timestamp ts(1234567890123456L);
    Value ts_val(ts);
    EXPECT_TRUE(ts_val.isTimestamp());
    EXPECT_EQ(ts.value, ts_val.asTimestamp().value);
    EXPECT_EQ(DataType::Timestamp, ts_val.type());
}

TEST_F(TypesTest, ValueSafeGetters) {
    Value null_val;
    EXPECT_EQ(0, null_val.getInt(0));
    EXPECT_EQ("default", null_val.getString("default"));
    EXPECT_FALSE(null_val.getBool(false));

    Value int_val(int32_t(42));
    EXPECT_EQ(42, int_val.getInt(0));
    EXPECT_EQ("default", int_val.getString("default"));  // Wrong type returns default
}

TEST_F(TypesTest, ValueComparison) {
    Value v1(int32_t(10));
    Value v2(int32_t(20));
    Value v3(int32_t(10));
    Value v4("hello");
    Value null_val;

    // Equality
    EXPECT_EQ(v1, v3);
    EXPECT_NE(v1, v2);
    EXPECT_NE(v1, v4);

    // Ordering
    EXPECT_LT(v1, v2);
    EXPECT_LE(v1, v2);
    EXPECT_LE(v1, v3);
    EXPECT_GT(v2, v1);
    EXPECT_GE(v2, v1);
    EXPECT_GE(v3, v1);

    // NULL comparisons
    EXPECT_LT(null_val, v1);
    EXPECT_GT(v1, null_val);
}

TEST_F(TypesTest, ValueSerialization) {
    // Test various types
    std::vector<Value> values = {
        Value(),                                     // NULL
        Value(true),                                 // Boolean
        Value(int32_t(42)),                          // Int32
        Value(int64_t(1234567890L)),                 // Int64
        Value(3.14f),                                // Float
        Value(3.14159),                              // Double
        Value("Hello, Lumen!"),                      // String
        Value(std::vector<byte>{0x01, 0x02, 0x03}),  // Blob
        Value(std::vector<float>{1.0f, 2.0f, 3.0f})  // Vector
    };

    for (const auto& original : values) {
        // Get serialized size
        size_t size = original.serializedSize();
        EXPECT_GT(size, 0u);

        // Serialize
        std::vector<byte> buffer(size);
        original.serialize(buffer.data());

        // Deserialize
        size_t offset = 0;
        Value deserialized = Value::deserialize(buffer.data(), offset);

        // Compare
        EXPECT_EQ(original, deserialized);
        EXPECT_EQ(size, offset);
    }
}

TEST_F(TypesTest, ValueToString) {
    EXPECT_EQ("NULL", Value().toString());
    EXPECT_EQ("true", Value(true).toString());
    EXPECT_EQ("false", Value(false).toString());
    EXPECT_EQ("42", Value(int32_t(42)).toString());
    // std::to_string behavior varies between platforms
    std::string double_str = Value(3.14).toString();
    EXPECT_TRUE(double_str == "3.140000" || double_str == "3.14");
    EXPECT_EQ("Hello", Value("Hello").toString());
    EXPECT_EQ("<blob:4 bytes>", Value(std::vector<byte>{1, 2, 3, 4}).toString());
    EXPECT_EQ("<vector:3 dims>", Value(std::vector<float>{1, 2, 3}).toString());
}

TEST_F(TypesTest, RowOperations) {
    Row row;
    EXPECT_TRUE(row.empty());
    EXPECT_EQ(0u, row.size());

    // Add values
    row.append(Value(int32_t(1)));
    row.append(Value("hello"));
    row.append(Value(3.14));

    EXPECT_FALSE(row.empty());
    EXPECT_EQ(3u, row.size());

    // Access values
    EXPECT_EQ(1, row[0].asInt());
    EXPECT_EQ("hello", row[1].asString());
    EXPECT_DOUBLE_EQ(3.14, row[2].asFloat());

    // Modify value
    row[0] = Value(int32_t(42));
    EXPECT_EQ(42, row[0].asInt());

    // Clear
    row.clear();
    EXPECT_TRUE(row.empty());
}

TEST_F(TypesTest, RowSerialization) {
    Row original;
    original.append(Value(int32_t(42)));
    original.append(Value("Hello"));
    original.append(Value(3.14));
    original.append(Value());  // NULL

    // Get serialized size
    size_t size = original.serializedSize();
    EXPECT_GT(size, 0u);

    // Serialize
    std::vector<byte> buffer(size);
    original.serialize(buffer.data());

    // Deserialize
    size_t offset = 0;
    Row deserialized = Row::deserialize(buffer.data(), offset);

    // Compare
    EXPECT_EQ(original.size(), deserialized.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(original[i], deserialized[i]);
    }
}

TEST_F(TypesTest, TypeTraits) {
    // Test numeric type trait
    static_assert(is_lumen_numeric_v<int32_t>);
    static_assert(is_lumen_numeric_v<float>);
    static_assert(is_lumen_numeric_v<double>);
    static_assert(!is_lumen_numeric_v<std::string>);
    static_assert(!is_lumen_numeric_v<bool>);
}

TEST_F(TypesTest, AlignmentUtility) {
    EXPECT_EQ(8u, align(5, 8));
    EXPECT_EQ(16u, align(9, 8));
    EXPECT_EQ(64u, align(33, 64));
    EXPECT_EQ(64u, align(64, 64));
}