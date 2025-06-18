#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS  // Allow strcpy on Windows
#endif

#include <lumen/lumen.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Minimal opaque structures for now (no dependencies)
struct lumen_storage_t {
    char* path;
    bool is_memory;
    bool is_open;
};

struct lumen_database_t {
    lumen_storage_t* storage;
    char* name;
};

struct lumen_schema_t {
    lumen_database_t* database;
};

struct lumen_query_builder_t {
    lumen_database_t* database;
    char* table_name;
    // TODO: Add query state
};

struct lumen_collection_t {
    // TODO: Add result data
    size_t count;
};

struct lumen_transaction_t {
    lumen_database_t* database;
    bool is_active;
};

// Global error message buffer
static char g_error_message[256] = "No error";

// Constants
static const char* const LUMEN_VERSION = "0.1.0";

// Error handling
extern "C" {

const char* lumen_error_message(LumenResult result) {
    switch (result) {
        case LUMEN_OK:
            return "No error";
        case LUMEN_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case LUMEN_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case LUMEN_ERROR_FILE_NOT_FOUND:
            return "File not found";
        case LUMEN_ERROR_FILE_CORRUPT:
            return "File corrupt";
        case LUMEN_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case LUMEN_ERROR_DISK_FULL:
            return "Disk full";
        case LUMEN_ERROR_TRANSACTION_ABORTED:
            return "Transaction aborted";
        case LUMEN_ERROR_DEADLOCK:
            return "Deadlock detected";
        case LUMEN_ERROR_CONSTRAINT_VIOLATION:
            return "Constraint violation";
        case LUMEN_ERROR_SCHEMA_MISMATCH:
            return "Schema mismatch";
        default:
            return "Unknown error";
    }
}

static void set_error(const char* message) {
    strncpy(g_error_message, message, sizeof(g_error_message) - 1);
    g_error_message[sizeof(g_error_message) - 1] = '\0';
}

// Version and initialization
const char* lumen_version_string(void) {
    return LUMEN_VERSION;
}

LumenResult lumen_initialize(void) {
    // Reset error state
    set_error("No error");

    // TODO: Initialize global state
    return LUMEN_OK;
}

void lumen_shutdown(void) {
    // TODO: Cleanup global state
}

// Storage operations
LumenStorage lumen_storage_create(const char* path) {
    if (!path) {
        set_error("Path cannot be null");
        return nullptr;
    }

    lumen_storage_t* storage = (lumen_storage_t*)std::malloc(sizeof(lumen_storage_t));
    if (!storage) {
        set_error("Out of memory");
        return nullptr;
    }

    // Check if memory database
    storage->is_memory = (std::strcmp(path, ":memory:") == 0);
    storage->is_open = true;

    // Copy path
    size_t path_len = std::strlen(path) + 1;
    storage->path = (char*)std::malloc(path_len);
    if (!storage->path) {
        std::free(storage);
        return nullptr;
    }
    std::strcpy(storage->path, path);

    return storage;
}

LumenResult lumen_storage_destroy(LumenStorage storage) {
    if (!storage) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }

    if (storage->path) {
        std::free(storage->path);
    }
    std::free(storage);
    return LUMEN_OK;
}

LumenResult lumen_storage_close(LumenStorage storage) {
    if (!storage) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }

    storage->is_open = false;
    return LUMEN_OK;
}

LumenResult lumen_storage_compact(LumenStorage storage) {
    if (!storage) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }

    // TODO: Implement actual compaction
    return LUMEN_OK;
}

// Database operations
LumenDatabase lumen_database_create(LumenStorage storage, const char* name) {
    if (!storage || !name || !storage->is_open) {
        return nullptr;
    }

    lumen_database_t* database = (lumen_database_t*)std::malloc(sizeof(lumen_database_t));
    if (!database) {
        return nullptr;
    }

    database->storage = storage;

    // Copy name
    size_t name_len = std::strlen(name) + 1;
    database->name = (char*)std::malloc(name_len);
    if (!database->name) {
        std::free(database);
        return nullptr;
    }
    std::strcpy(database->name, name);

    return database;
}

LumenResult lumen_database_destroy(LumenDatabase database) {
    if (!database) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }

    if (database->name) {
        std::free(database->name);
    }
    std::free(database);
    return LUMEN_OK;
}

LumenResult lumen_database_drop(LumenStorage storage, const char* name) {
    if (!storage || !name) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }

    // TODO: Implement actual database dropping
    return LUMEN_OK;
}

// Value creation helpers
LumenValue lumen_value_null(void) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_NULL;
    return value;
}

LumenValue lumen_value_int32(int32_t val) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_INT32;
    value.value.i32 = val;
    return value;
}

LumenValue lumen_value_int64(int64_t val) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_INT64;
    value.value.i64 = val;
    return value;
}

LumenValue lumen_value_double(double val) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_DOUBLE;
    value.value.f64 = val;
    return value;
}

LumenValue lumen_value_string(const char* val) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_STRING;
    value.value.string.data = val;
    value.value.string.length = val ? std::strlen(val) : 0;
    return value;
}

LumenValue lumen_value_blob(const void* data, size_t size) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_BLOB;
    value.value.blob.data = data;
    value.value.blob.length = size;
    return value;
}

LumenValue lumen_value_vector(const float* data, size_t dimensions) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_VECTOR;
    value.value.vector.data = data;
    value.value.vector.dimensions = dimensions;
    return value;
}

LumenValue lumen_value_boolean(bool val) {
    LumenValue value = {};
    value.type = LUMEN_TYPE_BOOLEAN;
    value.value.boolean = val;
    return value;
}

void lumen_free_string(char* str) {
    if (str) {
        std::free(str);
    }
}

// Schema operations
LumenSchema lumen_schema_create(LumenDatabase database) {
    if (!database) {
        return nullptr;
    }

    lumen_schema_t* schema = (lumen_schema_t*)std::malloc(sizeof(lumen_schema_t));
    if (!schema) {
        return nullptr;
    }

    schema->database = database;
    return schema;
}

LumenResult lumen_schema_destroy(LumenSchema schema) {
    if (!schema) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }

    std::free(schema);
    return LUMEN_OK;
}

LumenResult lumen_schema_create_table(LumenSchema schema, const char* table_name) {
    return (!schema || !table_name) ? LUMEN_ERROR_INVALID_ARGUMENT : LUMEN_OK;
}

LumenResult lumen_schema_drop_table(LumenSchema schema, const char* table_name) {
    return (!schema || !table_name) ? LUMEN_ERROR_INVALID_ARGUMENT : LUMEN_OK;
}

LumenResult lumen_schema_add_column(LumenSchema schema, const char* table_name,
                                    const char* column_name, LumenDataType type) {
    if (!schema || !table_name || !column_name) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }
    (void)type;  // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_schema_drop_column(LumenSchema schema, const char* table_name,
                                     const char* column_name) {
    return (!schema || !table_name || !column_name) ? LUMEN_ERROR_INVALID_ARGUMENT : LUMEN_OK;
}

LumenResult lumen_schema_create_index(LumenSchema schema, const char* table_name,
                                      const char* column_name, LumenIndexType type) {
    if (!schema || !table_name || !column_name) {
        return LUMEN_ERROR_INVALID_ARGUMENT;
    }
    (void)type;  // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_schema_drop_index(LumenSchema schema, const char* table_name,
                                    const char* column_name) {
    return (!schema || !table_name || !column_name) ? LUMEN_ERROR_INVALID_ARGUMENT : LUMEN_OK;
}

// Query builder operations
LumenQueryBuilder lumen_query_create(LumenDatabase database, const char* table_name) {
    if (!database || !table_name)
        return nullptr;

    lumen_query_builder_t* query =
        (lumen_query_builder_t*)std::malloc(sizeof(lumen_query_builder_t));
    if (!query)
        return nullptr;

    query->database = database;

    size_t name_len = std::strlen(table_name) + 1;
    query->table_name = (char*)std::malloc(name_len);
    if (!query->table_name) {
        std::free(query);
        return nullptr;
    }
    std::strcpy(query->table_name, table_name);

    return query;
}

LumenResult lumen_query_destroy(LumenQueryBuilder query) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;

    if (query->table_name) {
        std::free(query->table_name);
    }
    std::free(query);
    return LUMEN_OK;
}

LumenResult lumen_query_select(LumenQueryBuilder query, const char* columns) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)columns;  // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_query_where(LumenQueryBuilder query, const char* column, LumenOperator op,
                              const LumenValue* value) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)column;  // Unused parameter
    (void)op;      // Unused parameter
    (void)value;   // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_query_order_by(LumenQueryBuilder query, const char* column, bool ascending) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)column;     // Unused parameter
    (void)ascending;  // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_query_limit(LumenQueryBuilder query, size_t limit) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)limit;  // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_query_offset(LumenQueryBuilder query, size_t offset) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)offset;  // Unused parameter
    return LUMEN_OK;
}

char* lumen_query_to_sql(LumenQueryBuilder query) {
    if (!query || !query->table_name)
        return nullptr;

    // Simple SQL generation for debugging
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "SELECT * FROM %s", query->table_name);

    size_t len = std::strlen(buffer) + 1;
    char* result = (char*)std::malloc(len);
    if (result) {
        std::strcpy(result, buffer);
    }
    return result;
}

LumenCollection lumen_query_get(LumenQueryBuilder query) {
    if (!query)
        return nullptr;

    lumen_collection_t* collection = (lumen_collection_t*)std::malloc(sizeof(lumen_collection_t));
    if (!collection)
        return nullptr;

    collection->count = 0;
    return collection;
}

LumenResult lumen_query_insert(LumenQueryBuilder query, const LumenValue* values, size_t count) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)values;  // Unused parameter
    (void)count;   // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_query_update(LumenQueryBuilder query, const char* column,
                               const LumenValue* value) {
    if (!query)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)column;  // Unused parameter
    (void)value;   // Unused parameter
    return LUMEN_OK;
}

LumenResult lumen_query_delete(LumenQueryBuilder query) {
    return query ? LUMEN_OK : LUMEN_ERROR_INVALID_ARGUMENT;
}

// Collection operations
LumenResult lumen_collection_destroy(LumenCollection collection) {
    if (!collection)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    std::free(collection);
    return LUMEN_OK;
}

size_t lumen_collection_count(LumenCollection collection) {
    return collection ? collection->count : 0;
}

LumenResult lumen_collection_get_value(LumenCollection collection, size_t row, const char* column,
                                       LumenValue* value) {
    if (!collection || !value)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)row;     // Unused parameter
    (void)column;  // Unused parameter
    *value = lumen_value_null();
    return LUMEN_OK;
}

LumenResult lumen_collection_each(LumenCollection collection,
                                  void (*callback)(size_t row, const LumenValue* values,
                                                   size_t count, void* user_data),
                                  void* user_data) {
    if (!collection || !callback)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    (void)user_data;  // Unused parameter
    return LUMEN_OK;
}

// Transaction operations
LumenTransaction lumen_transaction_begin(LumenDatabase database) {
    if (!database)
        return nullptr;

    lumen_transaction_t* transaction =
        (lumen_transaction_t*)std::malloc(sizeof(lumen_transaction_t));
    if (!transaction)
        return nullptr;

    transaction->database = database;
    transaction->is_active = true;

    return transaction;
}

LumenResult lumen_transaction_commit(LumenTransaction transaction) {
    if (!transaction || !transaction->is_active)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    transaction->is_active = false;
    return LUMEN_OK;
}

LumenResult lumen_transaction_rollback(LumenTransaction transaction) {
    if (!transaction || !transaction->is_active)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    transaction->is_active = false;
    return LUMEN_OK;
}

LumenResult lumen_transaction_destroy(LumenTransaction transaction) {
    if (!transaction)
        return LUMEN_ERROR_INVALID_ARGUMENT;
    std::free(transaction);
    return LUMEN_OK;
}

}  // extern "C"
