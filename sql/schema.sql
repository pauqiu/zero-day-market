SET NAMES utf8mb4;

-- Schema for Jester's Chest (Mercado el Día Cero).
-- Run as root on VMDatos:  sudo mariadb jesterschest < sql/schema.sql
-- This first version has only what the catalog (HU-01) needs; the rest of
-- the logical model (offers, transfers, logs, ...) is added with each story.

CREATE TABLE IF NOT EXISTS users (
    user_id         INT UNSIGNED NOT NULL AUTO_INCREMENT,
    username        VARCHAR(32)  NOT NULL,
    email           VARCHAR(254) NOT NULL,
    password_hash   VARCHAR(255) NOT NULL,
    status          ENUM('pending', 'verified', 'active', 'locked') NOT NULL DEFAULT 'pending',
    failed_attempts SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    locked_until    DATETIME NULL,
    PRIMARY KEY (user_id),
    UNIQUE KEY uq_users_username (username),
    UNIQUE KEY uq_users_email (email)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
