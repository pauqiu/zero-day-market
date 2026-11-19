#include "infrastructure/db/mariadb_catalog_repository.h"

#include "infrastructure/db/database_error.h"

namespace {

constexpr const char* kCountSql = "SELECT COUNT(*) FROM objects";

// Every value from the client goes through placeholders (?), never through
// string concatenation. The description is shortened in SQL because
// LEFT() counts characters, so it never cuts a UTF-8 sequence in half.
constexpr const char* kListSql =
    "SELECT o.object_id, o.name, "
    "       CASE WHEN CHAR_LENGTH(o.description) > 140 "
    "            THEN CONCAT(LEFT(o.description, 140), '…') "
    "            ELSE o.description END, "
    "       u.username "
    "FROM objects o "
    "LEFT JOIN users u ON u.user_id = o.owner_id "
    "ORDER BY o.object_id "
    "LIMIT ? OFFSET ?";

constexpr unsigned int kListColumns = 4;

}  // namespace

MariaDbCatalogRepository::MariaDbCatalogRepository(MariaDbConnection& connection)
    : connection(connection)
{
}

long MariaDbCatalogRepository::countObjects()
{
    MariaDbStatement statement(connection, kCountSql);
    if (mysql_stmt_execute(statement.handle()) != 0) {
        statement.fail("execute");
    }

    long long count = 0;
    MYSQL_BIND result{};
    result.buffer_type = MYSQL_TYPE_LONGLONG;
    result.buffer = &count;
    if (mysql_stmt_bind_result(statement.handle(), &result) != 0) {
        statement.fail("bind_result");
    }
    if (mysql_stmt_fetch(statement.handle()) != 0) {
        statement.fail("fetch");
    }
    return static_cast<long>(count);
}

std::vector<CatalogItem> MariaDbCatalogRepository::listObjects(int limit, long offset)
{
    MariaDbStatement statement(connection, kListSql);
    MYSQL_STMT* stmt = statement.handle();

    if (mysql_stmt_field_count(stmt) != kListColumns) {
        throw DatabaseError("catalog query returned an unexpected number of columns");
    }

    int limitParam = limit;
    long long offsetParam = offset;
    MYSQL_BIND params[2]{};
    params[0].buffer_type = MYSQL_TYPE_LONG;
    params[0].buffer = &limitParam;
    params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    params[1].buffer = &offsetParam;
    if (mysql_stmt_bind_param(stmt, params) != 0) {
        statement.fail("bind_param");
    }
    if (mysql_stmt_execute(stmt) != 0) {
        statement.fail("execute");
    }

    // The integer is fetched directly; strings are fetched with a zero-size
    // buffer first to learn their length, then read with fetch_column.
    int objectID = 0;
    unsigned long lengths[kListColumns]{};
    my_bool nulls[kListColumns]{};
    MYSQL_BIND results[kListColumns]{};
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = &objectID;
    results[0].is_null = &nulls[0];
    for (unsigned int column = 1; column < kListColumns; ++column) {
        results[column].buffer_type = MYSQL_TYPE_STRING;
        results[column].buffer = nullptr;
        results[column].buffer_length = 0;
        results[column].length = &lengths[column];
        results[column].is_null = &nulls[column];
    }
    if (mysql_stmt_bind_result(stmt, results) != 0) {
        statement.fail("bind_result");
    }

    std::vector<CatalogItem> items;
    while (true) {
        const int status = mysql_stmt_fetch(stmt);
        if (status == MYSQL_NO_DATA) {
            break;
        }
        if (status == 1) {
            statement.fail("fetch");
        }
        // status is 0 or MYSQL_DATA_TRUNCATED (expected: string buffers are empty).

        CatalogItem item;
        item.objectID = objectID;
        item.name = statement.fetchString(1, lengths[1], nulls[1]).value_or("");
        item.shortDescription = statement.fetchString(2, lengths[2], nulls[2]).value_or("");
        item.ownerUsername = statement.fetchString(3, lengths[3], nulls[3]);
        items.push_back(std::move(item));
    }
    return items;
}
