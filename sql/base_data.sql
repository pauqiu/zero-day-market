SET NAMES utf8mb4;

-- Fixed data needed in every environment (production included).
-- Run as root on VMDatos after schema.sql.

INSERT INTO local (domain_name, commercial_name, website_name) VALUES
    ('jesterschest.internal', 'Jester''s Chest', 'www.jesterschest.internal');

INSERT INTO roles (role_id, name) VALUES
    (1, 'user'),
    (2, 'admin');

INSERT INTO permissions (permission_id, name, description) VALUES
    (1, 'view_audit_log',     'Consultar la bitácora global (HU-26)'),
    (2, 'view_incidents',     'Consultar los incidentes de seguridad (HU-28)'),
    (3, 'manage_suspensions', 'Suspender y reanudar intercambios (HU-30, HU-31)');

INSERT INTO role_permissions (role_id, permission_id) VALUES
    (2, 1), (2, 2), (2, 3);
