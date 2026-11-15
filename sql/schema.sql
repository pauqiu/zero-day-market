SET NAMES utf8mb4;

-- Logical model of Jester's Chest (Mercado el Día Cero).
-- Run as root on VMDatos:  sudo mariadb jesterschest < sql/schema.sql
-- All dates are stored in UTC; the application uses UTC_TIMESTAMP().
-- Tables are created in dependency order (referenced tables first).

-- ---------------------------------------------------------------------------
-- Local (store) data: a single row with the store's names.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS local (
    domain_name     VARCHAR(253) NOT NULL,
    commercial_name VARCHAR(100) NOT NULL,
    website_name    VARCHAR(253) NOT NULL,
    PRIMARY KEY (domain_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Roles and permissions (RBAC). Roles: user, admin.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS roles (
    role_id TINYINT UNSIGNED NOT NULL,
    name    VARCHAR(32) NOT NULL,
    PRIMARY KEY (role_id),
    UNIQUE KEY uq_roles_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS permissions (
    permission_id SMALLINT UNSIGNED NOT NULL,
    name          VARCHAR(64)  NOT NULL,
    description   VARCHAR(255) NOT NULL DEFAULT '',
    PRIMARY KEY (permission_id),
    UNIQUE KEY uq_permissions_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS role_permissions (
    role_id       TINYINT UNSIGNED  NOT NULL,
    permission_id SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY (role_id, permission_id),
    CONSTRAINT fk_rp_role       FOREIGN KEY (role_id)       REFERENCES roles (role_id),
    CONSTRAINT fk_rp_permission FOREIGN KEY (permission_id) REFERENCES permissions (permission_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Users and authentication.
-- status: pending  = email not verified yet (HU-06, HU-07)
--         verified = email verified, YubiKey not registered yet
--         active   = email verified and YubiKey registered (HU-10)
-- A temporary lockout (HU-09) is locked_until in the future.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
    user_id         INT UNSIGNED NOT NULL AUTO_INCREMENT,
    username        VARCHAR(32)  NOT NULL,
    email           VARCHAR(254) NOT NULL,
    password_hash   VARCHAR(255) NOT NULL,
    role_id         TINYINT UNSIGNED NOT NULL DEFAULT 1,
    status          ENUM('pending', 'verified', 'active') NOT NULL DEFAULT 'pending',
    failed_attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    locked_until    DATETIME NULL,
    PRIMARY KEY (user_id),
    UNIQUE KEY uq_users_username (username),
    UNIQUE KEY uq_users_email (email),
    CONSTRAINT fk_users_role FOREIGN KEY (role_id) REFERENCES roles (role_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Email verification tokens (HU-07). Only the SHA-256 of the token is
-- stored (hex); the token itself travels only in the email.
CREATE TABLE IF NOT EXISTS email_verifications (
    token_hash CHAR(64)     NOT NULL,
    user_id    INT UNSIGNED NOT NULL,
    expires_at DATETIME     NOT NULL,
    used_at    DATETIME     NULL,
    PRIMARY KEY (token_hash),
    KEY ix_ev_user (user_id),
    CONSTRAINT fk_ev_user FOREIGN KEY (user_id) REFERENCES users (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- YubiKey (Yubico OTP validated locally, HU-10/HU-11). The AES key and the
-- private ID are secrets: stored encrypted by the application with a master
-- key that is NOT in the database. The counters prevent OTP replay.
CREATE TABLE IF NOT EXISTS yubikeys (
    key_id           INT UNSIGNED NOT NULL AUTO_INCREMENT,
    user_id          INT UNSIGNED NOT NULL,
    public_id        CHAR(12)     NOT NULL,          -- modhex, first 12 chars of the OTP
    aes_key_enc      VARBINARY(128) NOT NULL,
    private_id_enc   VARBINARY(128) NOT NULL,
    last_counter     SMALLINT UNSIGNED NOT NULL DEFAULT 0,  -- usage counter (16 bits)
    last_session_use TINYINT UNSIGNED  NOT NULL DEFAULT 0,  -- session counter (8 bits)
    registered_at    DATETIME NOT NULL,
    PRIMARY KEY (key_id),
    UNIQUE KEY uq_yubikeys_public_id (public_id),   -- one key, one account (RF-2.22)
    UNIQUE KEY uq_yubikeys_user (user_id),          -- one key per user
    CONSTRAINT fk_yubikeys_user FOREIGN KEY (user_id) REFERENCES users (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Server-side sessions. The cookie carries the session ID; the database
-- stores only its SHA-256. mfa_verified = 0 between password and YubiKey.
CREATE TABLE IF NOT EXISTS sessions (
    session_id_hash CHAR(64)     NOT NULL,
    user_id         INT UNSIGNED NOT NULL,
    ip              VARCHAR(45)  NOT NULL,           -- fits IPv6
    csrf_token      CHAR(64)     NOT NULL,
    mfa_verified    BOOLEAN      NOT NULL DEFAULT FALSE,
    created_at      DATETIME     NOT NULL,
    last_seen_at    DATETIME     NOT NULL,
    expires_at      DATETIME     NOT NULL,
    PRIMARY KEY (session_id_hash),
    KEY ix_sessions_user (user_id),
    KEY ix_sessions_expires (expires_at),
    CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Objects and their documents.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS objects (
    object_id        INT UNSIGNED NOT NULL AUTO_INCREMENT,
    name             VARCHAR(100)  NOT NULL,
    description      VARCHAR(1000) NOT NULL DEFAULT '',
    owner_id         INT UNSIGNED NULL,   -- NULL = initial market inventory
    last_transfer_at DATETIME NULL,       -- NULL = never transferred
    PRIMARY KEY (object_id),
    KEY ix_objects_owner (owner_id),
    CONSTRAINT fk_objects_owner FOREIGN KEY (owner_id) REFERENCES users (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Authenticity documents (the "Certificate" entity of the ER diagram).
-- They belong to the object, not to the uploader (HU-16). The limit of 3
-- per object is enforced by the application inside a transaction.
CREATE TABLE IF NOT EXISTS documents (
    document_id   INT UNSIGNED NOT NULL AUTO_INCREMENT,
    object_id     INT UNSIGNED NOT NULL,
    original_name VARCHAR(255) NOT NULL,   -- sanitized, descriptive only
    stored_name   CHAR(64)     NOT NULL,   -- random name on disk
    mime_type     VARCHAR(100) NOT NULL,   -- real type detected with libmagic
    size_bytes    INT UNSIGNED NOT NULL,
    sha256        CHAR(64)     NOT NULL,   -- verified on every access (RF-1.21)
    uploaded_by   INT UNSIGNED NOT NULL,
    uploaded_at   DATETIME     NOT NULL,
    PRIMARY KEY (document_id),
    UNIQUE KEY uq_documents_stored_name (stored_name),
    KEY ix_documents_object (object_id),
    CONSTRAINT fk_documents_object   FOREIGN KEY (object_id)   REFERENCES objects (object_id),
    CONSTRAINT fk_documents_uploader FOREIGN KEY (uploaded_by) REFERENCES users (user_id),
    CONSTRAINT ck_documents_mime CHECK (mime_type IN
        ('text/html', 'application/pdf', 'image/svg+xml', 'image/png', 'image/jpeg'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Offers and transfer history.
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS offers (
    offer_id            INT UNSIGNED NOT NULL AUTO_INCREMENT,
    type                ENUM('trade', 'donation') NOT NULL,
    offerer_id          INT UNSIGNED NOT NULL,
    recipient_id        INT UNSIGNED NOT NULL,
    offered_object_id   INT UNSIGNED NOT NULL,
    requested_object_id INT UNSIGNED NULL,  -- NULL in donations
    status              ENUM('pending', 'completed', 'rejected', 'cancelled')
                        NOT NULL DEFAULT 'pending',
    created_at          DATETIME NOT NULL,
    resolved_at         DATETIME NULL,
    PRIMARY KEY (offer_id),
    KEY ix_offers_offerer (offerer_id, status),
    KEY ix_offers_recipient (recipient_id, status),
    KEY ix_offers_offered_object (offered_object_id),
    KEY ix_offers_requested_object (requested_object_id),
    CONSTRAINT fk_offers_offerer   FOREIGN KEY (offerer_id)          REFERENCES users (user_id),
    CONSTRAINT fk_offers_recipient FOREIGN KEY (recipient_id)        REFERENCES users (user_id),
    CONSTRAINT fk_offers_offered   FOREIGN KEY (offered_object_id)   REFERENCES objects (object_id),
    CONSTRAINT fk_offers_requested FOREIGN KEY (requested_object_id) REFERENCES objects (object_id),
    -- Defense in depth: the application validates these too (RN-4).
    CONSTRAINT ck_offers_parties CHECK (offerer_id <> recipient_id),
    CONSTRAINT ck_offers_shape CHECK (
        (type = 'donation' AND requested_object_id IS NULL) OR
        (type = 'trade' AND requested_object_id IS NOT NULL
                        AND requested_object_id <> offered_object_id)),
    CONSTRAINT ck_offers_resolved CHECK ((status = 'pending') = (resolved_at IS NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Public, append-only history (RN-9). A trade creates two rows with the
-- same offer_id; the initial allocation has no previous owner or offer.
CREATE TABLE IF NOT EXISTS transfers (
    transfer_id    INT UNSIGNED NOT NULL AUTO_INCREMENT,
    object_id      INT UNSIGNED NOT NULL,
    from_user_id   INT UNSIGNED NULL,
    to_user_id     INT UNSIGNED NOT NULL,
    type           ENUM('initial_allocation', 'trade', 'donation') NOT NULL,
    offer_id       INT UNSIGNED NULL,
    transferred_at DATETIME NOT NULL,
    PRIMARY KEY (transfer_id),
    KEY ix_transfers_object (object_id, transferred_at),
    CONSTRAINT fk_transfers_object FOREIGN KEY (object_id)    REFERENCES objects (object_id),
    CONSTRAINT fk_transfers_from   FOREIGN KEY (from_user_id) REFERENCES users (user_id),
    CONSTRAINT fk_transfers_to     FOREIGN KEY (to_user_id)   REFERENCES users (user_id),
    CONSTRAINT fk_transfers_offer  FOREIGN KEY (offer_id)     REFERENCES offers (offer_id),
    CONSTRAINT ck_transfers_shape CHECK (
        (type = 'initial_allocation' AND from_user_id IS NULL AND offer_id IS NULL) OR
        (type <> 'initial_allocation' AND from_user_id IS NOT NULL AND offer_id IS NOT NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- Administration: log, security incidents and trade suspensions.
-- ---------------------------------------------------------------------------
-- Append-only (HU-26/HU-27). category: auth, account, offer, document,
-- admin, mail.
CREATE TABLE IF NOT EXISTS audit_log (
    log_id      BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    created_at  DATETIME(3)  NOT NULL,
    ip          VARCHAR(45)  NULL,
    category    VARCHAR(32)  NOT NULL,
    description VARCHAR(500) NOT NULL,
    user_id     INT UNSIGNED NULL,
    details     TEXT NULL,
    PRIMARY KEY (log_id),
    KEY ix_audit_created (created_at),
    KEY ix_audit_category (category, created_at),
    KEY ix_audit_user (user_id, created_at),
    CONSTRAINT fk_audit_user FOREIGN KEY (user_id) REFERENCES users (user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Append-only (HU-28/HU-29).
CREATE TABLE IF NOT EXISTS security_incidents (
    incident_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    created_at  DATETIME(3)  NOT NULL,
    ip          VARCHAR(45)  NULL,
    category    VARCHAR(32)  NOT NULL,
    description VARCHAR(500) NOT NULL,
    user_id     INT UNSIGNED NULL,
    object_id   INT UNSIGNED NULL,
    document_id INT UNSIGNED NULL,
    details     TEXT NULL,
    PRIMARY KEY (incident_id),
    KEY ix_incidents_created (created_at),
    KEY ix_incidents_category (category, created_at),
    CONSTRAINT fk_incidents_user     FOREIGN KEY (user_id)     REFERENCES users (user_id),
    CONSTRAINT fk_incidents_object   FOREIGN KEY (object_id)   REFERENCES objects (object_id),
    CONSTRAINT fk_incidents_document FOREIGN KEY (document_id) REFERENCES documents (document_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- HU-30/HU-31. A suspension is active when
-- starts_at <= now < ends_at AND ended_at IS NULL.
CREATE TABLE IF NOT EXISTS trade_suspensions (
    suspension_id INT UNSIGNED NOT NULL AUTO_INCREMENT,
    starts_at     DATETIME     NOT NULL,
    ends_at       DATETIME     NOT NULL,
    reason        VARCHAR(500) NOT NULL,
    created_by    INT UNSIGNED NOT NULL,
    ended_at      DATETIME     NULL,   -- set when an admin ends it early
    ended_by      INT UNSIGNED NULL,
    PRIMARY KEY (suspension_id),
    KEY ix_suspensions_window (starts_at, ends_at),
    CONSTRAINT fk_suspensions_creator FOREIGN KEY (created_by) REFERENCES users (user_id),
    CONSTRAINT fk_suspensions_ender   FOREIGN KEY (ended_by)   REFERENCES users (user_id),
    CONSTRAINT ck_suspensions_window CHECK (ends_at > starts_at),
    CONSTRAINT ck_suspensions_end CHECK ((ended_at IS NULL) = (ended_by IS NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
