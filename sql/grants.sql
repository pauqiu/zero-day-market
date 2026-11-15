-- Least-privilege grants for the application account. Run as root on
-- VMDatos after schema.sql. Replaces the broad grant on jesterschest.*.
-- What each table does NOT allow is as important as what it allows:
--   * transfers, audit_log, security_incidents: INSERT and SELECT only, so
--     even a compromised CGI cannot alter the history or erase its tracks.
--   * objects: only owner_id and last_transfer_at can change; the initial
--     inventory is loaded by root, so the application cannot create objects.
--   * local, roles, permissions: read-only.
--   * Nothing grants DROP, ALTER, CREATE or GRANT OPTION.

REVOKE ALL PRIVILEGES, GRANT OPTION FROM 'jc_app'@'192.168.160.10';

GRANT SELECT ON jesterschest.local            TO 'jc_app'@'192.168.160.10';
GRANT SELECT ON jesterschest.roles            TO 'jc_app'@'192.168.160.10';
GRANT SELECT ON jesterschest.permissions      TO 'jc_app'@'192.168.160.10';
GRANT SELECT ON jesterschest.role_permissions TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.users TO 'jc_app'@'192.168.160.10';
GRANT UPDATE (password_hash, status, failed_attempts, locked_until)
    ON jesterschest.users TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.email_verifications TO 'jc_app'@'192.168.160.10';
GRANT UPDATE (used_at) ON jesterschest.email_verifications TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.yubikeys TO 'jc_app'@'192.168.160.10';
GRANT UPDATE (last_counter, last_session_use)
    ON jesterschest.yubikeys TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT, UPDATE, DELETE ON jesterschest.sessions TO 'jc_app'@'192.168.160.10';

GRANT SELECT ON jesterschest.objects TO 'jc_app'@'192.168.160.10';
GRANT UPDATE (owner_id, last_transfer_at) ON jesterschest.objects TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.documents TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.offers TO 'jc_app'@'192.168.160.10';
GRANT UPDATE (status, resolved_at) ON jesterschest.offers TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.transfers          TO 'jc_app'@'192.168.160.10';
GRANT SELECT, INSERT ON jesterschest.audit_log          TO 'jc_app'@'192.168.160.10';
GRANT SELECT, INSERT ON jesterschest.security_incidents TO 'jc_app'@'192.168.160.10';

GRANT SELECT, INSERT ON jesterschest.trade_suspensions TO 'jc_app'@'192.168.160.10';
GRANT UPDATE (ended_at, ended_by) ON jesterschest.trade_suspensions TO 'jc_app'@'192.168.160.10';
