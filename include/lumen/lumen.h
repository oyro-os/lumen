#ifndef LUMEN_H
#define LUMEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Version information
#define LUMEN_VERSION_MAJOR 1
#define LUMEN_VERSION_MINOR 0
#define LUMEN_VERSION_PATCH 0

// Platform detection and export macros
#ifdef _WIN32
    #ifdef LUMEN_SHARED_LIB
        #define LUMEN_API __declspec(dllexport)
    #elif defined(LUMEN_STATIC_LIB)
        #define LUMEN_API
    #else
        #define LUMEN_API __declspec(dllimport)
    #endif
#else
    #ifdef LUMEN_SHARED_LIB
        #define LUMEN_API __attribute__((visibility("default")))
    #else
        #define LUMEN_API
    #endif
#endif

// Forward declarations - opaque handles
typedef struct lumen_storage_t* LumenStorage;
typedef struct lumen_database_t* LumenDatabase;
typedef struct lumen_schema_t* LumenSchema;
typedef struct lumen_table_t* LumenTable;
typedef struct lumen_query_builder_t* LumenQueryBuilder;
typedef struct lumen_collection_t* LumenCollection;
typedef struct lumen_transaction_t* LumenTransaction;

// Error codes
typedef enum {
    LUMEN_OK = 0,
    LUMEN_ERROR_INVALID_ARGUMENT = -1,
    LUMEN_ERROR_OUT_OF_MEMORY = -2,
    LUMEN_ERROR_FILE_NOT_FOUND = -3,
    LUMEN_ERROR_FILE_CORRUPT = -4,
    LUMEN_ERROR_PERMISSION_DENIED = -5,
    LUMEN_ERROR_DISK_FULL = -6,
    LUMEN_ERROR_TRANSACTION_ABORTED = -7,
    LUMEN_ERROR_DEADLOCK = -8,
    LUMEN_ERROR_CONSTRAINT_VIOLATION = -9,
    LUMEN_ERROR_SCHEMA_MISMATCH = -10
} LumenResult;

// Data types
typedef enum {
    LUMEN_TYPE_NULL = 0,
    LUMEN_TYPE_INT8 = 1,
    LUMEN_TYPE_INT16 = 2,
    LUMEN_TYPE_INT32 = 3,
    LUMEN_TYPE_INT64 = 4,
    LUMEN_TYPE_UINT8 = 5,
    LUMEN_TYPE_UINT16 = 6,
    LUMEN_TYPE_UINT32 = 7,
    LUMEN_TYPE_UINT64 = 8,
    LUMEN_TYPE_FLOAT = 9,
    LUMEN_TYPE_DOUBLE = 10,
    LUMEN_TYPE_STRING = 11,
    LUMEN_TYPE_BLOB = 12,
    LUMEN_TYPE_VECTOR = 13,
    LUMEN_TYPE_TIMESTAMP = 14,
    LUMEN_TYPE_BOOLEAN = 15
} LumenDataType;

// Index types
typedef enum {
    LUMEN_INDEX_BTREE = 0,
    LUMEN_INDEX_HASH = 1,
    LUMEN_INDEX_VECTOR_HNSW = 2,
    LUMEN_INDEX_VECTOR_IVF = 3,
    LUMEN_INDEX_VECTOR_LSH = 4,
    LUMEN_INDEX_FULLTEXT = 5
} LumenIndexType;

// Comparison operators
typedef enum {
    LUMEN_OP_EQ = 0,    // =
    LUMEN_OP_NE = 1,    // !=
    LUMEN_OP_LT = 2,    // <
    LUMEN_OP_LE = 3,    // <=
    LUMEN_OP_GT = 4,    // >
    LUMEN_OP_GE = 5,    // >=
    LUMEN_OP_IN = 6,    // IN
    LUMEN_OP_NOT_IN = 7,    // NOT IN
    LUMEN_OP_LIKE = 8,      // LIKE
    LUMEN_OP_NOT_LIKE = 9,  // NOT LIKE
    LUMEN_OP_SIMILAR = 10   // Vector similarity
} LumenOperator;

// Value structure for dynamic typing
typedef struct {
    LumenDataType type;
    union {
        int8_t i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        uint8_t u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
        float f32;
        double f64;
        bool boolean;
        struct {
            const char* data;
            size_t length;
        } string;
        struct {
            const void* data;
            size_t length;
        } blob;
        struct {
            const float* data;
            size_t dimensions;
        } vector;
        int64_t timestamp;
    } value;
} LumenValue;

// Storage operations
LUMEN_API LumenStorage lumen_storage_create(const char* path);
LUMEN_API LumenResult lumen_storage_destroy(LumenStorage storage);
LUMEN_API LumenResult lumen_storage_close(LumenStorage storage);
LUMEN_API LumenResult lumen_storage_compact(LumenStorage storage);

// Database operations
LUMEN_API LumenDatabase lumen_database_create(LumenStorage storage, const char* name);
LUMEN_API LumenResult lumen_database_destroy(LumenDatabase database);
LUMEN_API LumenResult lumen_database_drop(LumenStorage storage, const char* name);

// Schema operations
LUMEN_API LumenSchema lumen_schema_create(LumenDatabase database);
LUMEN_API LumenResult lumen_schema_destroy(LumenSchema schema);
LUMEN_API LumenResult lumen_schema_create_table(LumenSchema schema, const char* table_name);
LUMEN_API LumenResult lumen_schema_drop_table(LumenSchema schema, const char* table_name);
LUMEN_API LumenResult lumen_schema_add_column(LumenSchema schema, const char* table_name, 
                                             const char* column_name, LumenDataType type);
LUMEN_API LumenResult lumen_schema_drop_column(LumenSchema schema, const char* table_name, 
                                              const char* column_name);
LUMEN_API LumenResult lumen_schema_create_index(LumenSchema schema, const char* table_name,
                                               const char* column_name, LumenIndexType type);
LUMEN_API LumenResult lumen_schema_drop_index(LumenSchema schema, const char* table_name,
                                             const char* column_name);

// Query builder operations
LUMEN_API LumenQueryBuilder lumen_query_create(LumenDatabase database, const char* table_name);
LUMEN_API LumenResult lumen_query_destroy(LumenQueryBuilder query);
LUMEN_API LumenResult lumen_query_select(LumenQueryBuilder query, const char* columns);
LUMEN_API LumenResult lumen_query_where(LumenQueryBuilder query, const char* column, 
                                       LumenOperator op, const LumenValue* value);
LUMEN_API LumenResult lumen_query_order_by(LumenQueryBuilder query, const char* column, bool ascending);
LUMEN_API LumenResult lumen_query_limit(LumenQueryBuilder query, size_t limit);
LUMEN_API LumenResult lumen_query_offset(LumenQueryBuilder query, size_t offset);
LUMEN_API char* lumen_query_to_sql(LumenQueryBuilder query);
LUMEN_API LumenCollection lumen_query_get(LumenQueryBuilder query);
LUMEN_API LumenResult lumen_query_insert(LumenQueryBuilder query, const LumenValue* values, size_t count);
LUMEN_API LumenResult lumen_query_update(LumenQueryBuilder query, const char* column, const LumenValue* value);
LUMEN_API LumenResult lumen_query_delete(LumenQueryBuilder query);

// Collection operations
LUMEN_API LumenResult lumen_collection_destroy(LumenCollection collection);
LUMEN_API size_t lumen_collection_count(LumenCollection collection);
LUMEN_API LumenResult lumen_collection_get_value(LumenCollection collection, size_t row, 
                                                const char* column, LumenValue* value);
LUMEN_API LumenResult lumen_collection_each(LumenCollection collection,
                                           void (*callback)(size_t row, const LumenValue* values, size_t count, void* user_data),
                                           void* user_data);

// Transaction operations
LUMEN_API LumenTransaction lumen_transaction_begin(LumenDatabase database);
LUMEN_API LumenResult lumen_transaction_commit(LumenTransaction transaction);
LUMEN_API LumenResult lumen_transaction_rollback(LumenTransaction transaction);
LUMEN_API LumenResult lumen_transaction_destroy(LumenTransaction transaction);

// Utility functions
LUMEN_API const char* lumen_error_message(LumenResult result);
LUMEN_API const char* lumen_version_string(void);
LUMEN_API LumenResult lumen_initialize(void);
LUMEN_API void lumen_shutdown(void);

// Memory management helpers
LUMEN_API void lumen_free_string(char* str);
LUMEN_API LumenValue lumen_value_null(void);
LUMEN_API LumenValue lumen_value_int32(int32_t value);
LUMEN_API LumenValue lumen_value_int64(int64_t value);
LUMEN_API LumenValue lumen_value_double(double value);
LUMEN_API LumenValue lumen_value_string(const char* value);
LUMEN_API LumenValue lumen_value_blob(const void* data, size_t length);
LUMEN_API LumenValue lumen_value_vector(const float* data, size_t dimensions);
LUMEN_API LumenValue lumen_value_boolean(bool value);

#ifdef __cplusplus
}
#endif

#endif // LUMEN_H