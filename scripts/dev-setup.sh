#!/usr/bin/env bash
# One-time setup of the local development environment on Ubuntu 24.04
# (WSL2). It mirrors production: MariaDB with mandatory TLS, the same
# paths for the application configuration, and Apache running app.cgi.
#
#   sudo apt install build-essential cmake pkg-config libmariadb-dev \
#                    mariadb-server apache2 openssl libssl-dev libyubikey-dev \
#                    libcurl4-openssl-dev
#   pip install aiosmtpd --break-system-packages   (for scripts/dev-smtp.py)
#   ./scripts/dev-setup.sh            # set up (keeps existing data)
#   ./scripts/dev-setup.sh --reset    # drop the local database and start over
#
# Local password for jc_app: DB_PASS environment variable, "devpass" by default.
# Never use these values outside your machine.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
DB_PASS="${DB_PASS:-devpass}"
TLS_DIR=/etc/mysql/jesterschest-tls
CONF_DIR=/etc/jesterschest
WEB_ROOT=/var/www/jesterschest

step() { printf '\n==> %s\n' "$1"; }

step "TLS certificate for local MariaDB (self-signed, IP 127.0.0.1)"
sudo install -d -m 0755 "$TLS_DIR"
if [ ! -f "$TLS_DIR/server.crt" ]; then
    sudo openssl req -x509 -newkey rsa:3072 -nodes -days 365 \
        -keyout "$TLS_DIR/server.key" -out "$TLS_DIR/server.crt" \
        -subj "/CN=dev-mariadb" -addext "subjectAltName=IP:127.0.0.1" 2>/dev/null
fi
sudo chown mysql:mysql "$TLS_DIR/server.key"
sudo chmod 600 "$TLS_DIR/server.key"

step "MariaDB configuration (same options as VMDatos)"
sudo tee /etc/mysql/mariadb.conf.d/90-jesterschest.cnf > /dev/null << CNF
[mysqld]
bind-address = 127.0.0.1
skip_name_resolve = ON
require_secure_transport = ON
ssl_cert = $TLS_DIR/server.crt
ssl_key = $TLS_DIR/server.key
local_infile = 0
character-set-server = utf8mb4
collation-server = utf8mb4_unicode_ci
CNF
sudo service mariadb restart > /dev/null

step "Database, application user and schema"
if [ "${1:-}" = "--reset" ]; then
    sudo mariadb -e "DROP DATABASE IF EXISTS jesterschest;"
fi
sudo mariadb << SQL
CREATE DATABASE IF NOT EXISTS jesterschest
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'jc_app'@'127.0.0.1' IDENTIFIED BY '$DB_PASS';
ALTER USER 'jc_app'@'127.0.0.1' IDENTIFIED BY '$DB_PASS';
SQL
sudo mariadb jesterschest < "$REPO/sql/schema.sql"
if [ "$(sudo mariadb -N jesterschest -e 'SELECT COUNT(*) FROM roles')" = "0" ]; then
    sudo mariadb jesterschest < "$REPO/sql/base_data.sql"
    sudo mariadb jesterschest < "$REPO/sql/dev_seed.sql"
fi
# Same grants as production, for the local client address.
sed 's/192\.168\.160\.10/127.0.0.1/g' "$REPO/sql/grants.sql" | sudo mariadb

step "Application configuration in /etc/jesterschest (same path as production)"
sudo install -d -m 0750 -o root -g www-data "$CONF_DIR"
sudo install -m 0644 -o root -g root "$TLS_DIR/server.crt" "$CONF_DIR/mariadb-ca.crt"
sudo tee "$CONF_DIR/db.conf" > /dev/null << CONF
host = 127.0.0.1
port = 3306
user = jc_app
password = $DB_PASS
database = jesterschest
ssl_ca = $CONF_DIR/mariadb-ca.crt
CONF
sudo chown root:www-data "$CONF_DIR/db.conf"
sudo chmod 0640 "$CONF_DIR/db.conf"

# Master key that seals the YubiKey secrets. Created once; never replaced,
# because the sealed keys in the database could no longer be opened.
if [ ! -f "$CONF_DIR/yubikey-master.key" ]; then
    openssl rand -hex 32 | sudo tee "$CONF_DIR/yubikey-master.key" > /dev/null
fi
sudo chown root:www-data "$CONF_DIR/yubikey-master.key"
sudo chmod 0640 "$CONF_DIR/yubikey-master.key"

step "Mail: local SMTP server certificate and mail.conf (scripts/dev-smtp.py)"
SMTP_DIR="$HOME/.local/share/jesterschest-dev-smtp"
mkdir -p "$SMTP_DIR"
if [ ! -f "$SMTP_DIR/server.crt" ]; then
    openssl req -x509 -newkey rsa:2048 -nodes -days 365 -subj "/CN=dev-smtp" \
        -keyout "$SMTP_DIR/server.key" -out "$SMTP_DIR/server.crt" 2>/dev/null
    chmod 600 "$SMTP_DIR/server.key"
fi
# Same kind of pin as in production: SHA-256 of the server public key.
PIN="sha256//$(openssl x509 -in "$SMTP_DIR/server.crt" -noout -pubkey \
    | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | openssl base64)"
sudo tee "$CONF_DIR/mail.conf" > /dev/null << CONF
url = smtp://127.0.0.1:2525
username = tienda@jesterschest.internal
password = dev-password
from = tienda@jesterschest.internal
pinned_public_key = $PIN
app_url = http://localhost/app.cgi
CONF
sudo chown root:www-data "$CONF_DIR/mail.conf"
sudo chmod 0640 "$CONF_DIR/mail.conf"

step "Apache: CGI module and site"
sudo install -d -m 0755 "$WEB_ROOT/static"
sudo a2enmod -q cgid > /dev/null
sudo install -m 0644 "$REPO/deploy/apache/jesterschest-dev.conf" \
    /etc/apache2/sites-available/jesterschest-dev.conf
sudo a2dissite -q 000-default > /dev/null 2>&1 || true
sudo a2ensite -q jesterschest-dev > /dev/null
sudo apache2ctl configtest
sudo service apache2 restart > /dev/null

step "Done. Now build and deploy with ./scripts/dev-deploy.sh"
