#include "infrastructure/db/mariadb_object_detail_repository.h"

#include "infrastructure/db/database_error.h"

namespace {

// Dates are stored in UTC. TIMESTAMPDIFF from the epoch converts them to UTC
// epoch seconds without depending on the session's time zone (UNIX_TIMESTAMP
// would apply it). The object ID always travels as a bound parameter.
constexpr const char* kFindSql =
    "SELECT o.object_id, o.name, o.description, u.username, "
    "       TIMESTAMPDIFF(SECOND, '1970-01-01 00:00:00', o.last_transfer_at) "
    "FROM objects o "
    "LEFT JOIN users u ON u.user_id = o.owner_id "
    "WHERE o.object_id = ?";

constexpr const char* kDocumentsSql =
    "SELECT document_id, original_name, mime_type, size_bytes, "
    "       TIMESTAMPDIFF(SECOND, '1970-01-01 00:00:00', uploaded_at) "
    "FROM documents "
    "WHERE object_id = ? "
    "ORDER BY uploaded_at, document_id";

void bindObjectID(MariaDbStatement& statement, int& objectID, MYSQL_BIND& param)
{
    param = MYSQL_BIND{};
    param.buffer_type = MYSQL_TYPE_LONG;
    param.buffer = &objectID;
    if (mysql_stmt_bind_param(statement.handle(), &param) != 0) {
        statement.fail("bind_param");
    }
    if (mysql_stmt_execute(statement.handle()) != 0) {
        statement.fail("execute");
    }
}

}  // namespace

MariaDbObjectDetailRepository::MariaDbObjectDetailRepository(MariaDbConnection& connection)
    : connection(connection)
{
}

std::optional<ObjectRecord> MariaDbObjectDetailRepository::findObject(int objectID)
{
    MariaDbStatement statement(connection, kFindSql);
    MYSQL_STMT* stmt = statement.handle();
    if (mysql_stmt_field_count(stmt) != 5) {
        throw DatabaseError("object query returned an unexpected number of columns");
    }

    int idParam = objectID;
    MYSQL_BIND param;
    bindObjectID(statement, idParam, param);

    int id = 0;
    long long lastTransfer = 0;
    unsigned long lengths[5]{};
    my_bool nulls[5]{};
    MYSQL_BIND results[5]{};
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = &id;
    results[0].is_null = &nulls[0];
    for (unsigned int column = 1; column <= 3; ++column) {
        results[column].buffer_type = MYSQL_TYPE_STRING;
        results[column].length = &lengths[column];
        results[column].is_null = &nulls[column];
    }
    results[4].buffer_type = MYSQL_TYPE_LONGLONG;
    results[4].buffer = &lastTransfer;
    results[4].is_null = &nulls[4];
    if (mysql_stmt_bind_result(stmt, results) != 0) {
        statement.fail("bind_result");
    }

    const int status = mysql_stmt_fetch(stmt);
    if (status == MYSQL_NO_DATA) {
        return std::nullopt;
    }
    if (status == 1) {
        statement.fail("fetch");
    }

    ObjectRecord record;
    record.objectID = id;
    record.name = statement.fetchString(1, lengths[1], nulls[1]).value_or("");
    record.description = statement.fetchString(2, lengths[2], nulls[2]).value_or("");
    record.ownerUsername = statement.fetchString(3, lengths[3], nulls[3]);
    if (!nulls[4]) {
        record.lastTransferAt = lastTransfer;
    }
    return record;
}

std::vector<DocumentSummary> MariaDbObjectDetailRepository::listDocuments(int objectID)
{
    MariaDbStatement statement(connection, kDocumentsSql);
    MYSQL_STMT* stmt = statement.handle();
    if (mysql_stmt_field_count(stmt) != 5) {
        throw DatabaseError("documents query returned an unexpected number of columns");
    }

    int idParam = objectID;
    MYSQL_BIND param;
    bindObjectID(statement, idParam, param);

    int documentID = 0;
    long long sizeBytes = 0;
    long long uploadedAt = 0;
    unsigned long lengths[5]{};
    my_bool nulls[5]{};
    MYSQL_BIND results[5]{};
    results[0].buffer_type = MYSQL_TYPE_LONG;
    results[0].buffer = &documentID;
    results[0].is_null = &nulls[0];
    for (unsigned int column = 1; column <= 2; ++column) {
        results[column].buffer_type = MYSQL_TYPE_STRING;
        results[column].length = &lengths[column];
        results[column].is_null = &nulls[column];
    }
    results[3].buffer_type = MYSQL_TYPE_LONGLONG;
    results[3].buffer = &sizeBytes;
    results[3].is_null = &nulls[3];
    results[4].buffer_type = MYSQL_TYPE_LONGLONG;
    results[4].buffer = &uploadedAt;
    results[4].is_null = &nulls[4];
    if (mysql_stmt_bind_result(stmt, results) != 0) {
        statement.fail("bind_result");
    }

    std::vector<DocumentSummary> documents;
    while (true) {
        const int status = mysql_stmt_fetch(stmt);
        if (status == MYSQL_NO_DATA) {
            break;
        }
        if (status == 1) {
            statement.fail("fetch");
        }
        DocumentSummary document;
        document.documentID = documentID;
        document.originalName = statement.fetchString(1, lengths[1], nulls[1]).value_or("");
        document.mimeType = statement.fetchString(2, lengths[2], nulls[2]).value_or("");
        document.sizeBytes = static_cast<long>(sizeBytes);
        document.uploadedAt = uploadedAt;
        documents.push_back(std::move(document));
    }
    return documents;
}
