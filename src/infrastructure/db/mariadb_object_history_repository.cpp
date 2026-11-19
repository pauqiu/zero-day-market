#include "infrastructure/db/mariadb_object_history_repository.h"

#include "infrastructure/db/database_error.h"

namespace {

constexpr const char* kNameSql = "SELECT name FROM objects WHERE object_id = ?";

// One row per transfer of the object. For a trade, the other transfer of the
// same offer (other.object_id <> t.object_id) gives the object received in
// exchange (RF-1.12). Dates become UTC epoch seconds, as in the detail query.
constexpr const char* kTransfersSql =
    "SELECT TIMESTAMPDIFF(SECOND, '1970-01-01 00:00:00', t.transferred_at), "
    "       t.type, f.username, d.username, other.object_id, o2.name "
    "FROM transfers t "
    "JOIN users d ON d.user_id = t.to_user_id "
    "LEFT JOIN users f ON f.user_id = t.from_user_id "
    "LEFT JOIN transfers other ON t.type = 'trade' "
    "                         AND other.offer_id = t.offer_id "
    "                         AND other.object_id <> t.object_id "
    "LEFT JOIN objects o2 ON o2.object_id = other.object_id "
    "WHERE t.object_id = ? "
    "ORDER BY t.transferred_at DESC, t.transfer_id DESC";

constexpr unsigned int kTransferColumns = 6;

TransferType parseType(const std::string& value)
{
    if (value == "initial_allocation") return TransferType::InitialAllocation;
    if (value == "trade") return TransferType::Trade;
    if (value == "donation") return TransferType::Donation;
    throw DatabaseError("unknown transfer type in the database");
}

void executeWithObjectID(MariaDbStatement& statement, int& objectID, MYSQL_BIND& param)
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

MariaDbObjectHistoryRepository::MariaDbObjectHistoryRepository(MariaDbConnection& connection)
    : connection(connection)
{
}

std::optional<std::string> MariaDbObjectHistoryRepository::findObjectName(int objectID)
{
    MariaDbStatement statement(connection, kNameSql);
    int idParam = objectID;
    MYSQL_BIND param;
    executeWithObjectID(statement, idParam, param);

    unsigned long length = 0;
    my_bool isNull = 0;
    MYSQL_BIND result{};
    result.buffer_type = MYSQL_TYPE_STRING;
    result.length = &length;
    result.is_null = &isNull;
    if (mysql_stmt_bind_result(statement.handle(), &result) != 0) {
        statement.fail("bind_result");
    }

    const int status = mysql_stmt_fetch(statement.handle());
    if (status == MYSQL_NO_DATA) {
        return std::nullopt;
    }
    if (status == 1) {
        statement.fail("fetch");
    }
    return statement.fetchString(0, length, isNull).value_or("");
}

std::vector<TransferEntry> MariaDbObjectHistoryRepository::listTransfers(int objectID)
{
    MariaDbStatement statement(connection, kTransfersSql);
    MYSQL_STMT* stmt = statement.handle();
    if (mysql_stmt_field_count(stmt) != kTransferColumns) {
        throw DatabaseError("history query returned an unexpected number of columns");
    }

    int idParam = objectID;
    MYSQL_BIND param;
    executeWithObjectID(statement, idParam, param);

    long long transferredAt = 0;
    int exchangedID = 0;
    unsigned long lengths[kTransferColumns]{};
    my_bool nulls[kTransferColumns]{};
    MYSQL_BIND results[kTransferColumns]{};
    results[0].buffer_type = MYSQL_TYPE_LONGLONG;
    results[0].buffer = &transferredAt;
    results[0].is_null = &nulls[0];
    for (unsigned int column : {1u, 2u, 3u, 5u}) {
        results[column].buffer_type = MYSQL_TYPE_STRING;
        results[column].length = &lengths[column];
        results[column].is_null = &nulls[column];
    }
    results[4].buffer_type = MYSQL_TYPE_LONG;
    results[4].buffer = &exchangedID;
    results[4].is_null = &nulls[4];
    if (mysql_stmt_bind_result(stmt, results) != 0) {
        statement.fail("bind_result");
    }

    std::vector<TransferEntry> entries;
    while (true) {
        const int status = mysql_stmt_fetch(stmt);
        if (status == MYSQL_NO_DATA) {
            break;
        }
        if (status == 1) {
            statement.fail("fetch");
        }
        TransferEntry entry;
        entry.transferredAt = transferredAt;
        entry.type = parseType(statement.fetchString(1, lengths[1], nulls[1]).value_or(""));
        entry.fromUsername = statement.fetchString(2, lengths[2], nulls[2]);
        entry.toUsername = statement.fetchString(3, lengths[3], nulls[3]).value_or("");
        if (!nulls[4]) {
            entry.exchangedObjectID = exchangedID;
            entry.exchangedObjectName = statement.fetchString(5, lengths[5], nulls[5]);
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}
