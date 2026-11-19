#!/usr/bin/env bash
# Installs or updates the application on VMServicios (Rocky Linux 9).
# Run from the copied dist/ directory:   sudo ./install-vmservicios.sh
#
# Prerequisites (done once, by hand):
#   * /etc/pki/tls/certs/jesterschest.crt   certificate from the course CA
#   * /etc/pki/tls/private/jesterschest.key the key generated with the CSR
#   * /etc/jesterschest/mariadb-ca.crt      certificate of VMDatos
#   * libyubikey from EPEL (see the installation document):
#       sudo dnf install epel-release && sudo dnf install libyubikey
#   * /etc/jesterschest/mail.conf           from mail.conf.example (see mail-pin.sh)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
CONF_DIR=/etc/jesterschest
WEB_ROOT=/var/www/jesterschest

[ "$(id -u)" -eq 0 ] || { echo "error: run with sudo" >&2; exit 1; }

step() { printf '\n==> %s\n' "$1"; }

step "Verifying the bundle checksums"
(cd "$HERE" && sha256sum --quiet -c SHA256SUMS)

step "Checking prerequisites"
for f in /etc/pki/tls/certs/jesterschest.crt /etc/pki/tls/private/jesterschest.key \
         "$CONF_DIR/mariadb-ca.crt"; do
    [ -f "$f" ] || { echo "error: missing $f" >&2; exit 1; }
done
cert_key=$(openssl x509 -in /etc/pki/tls/certs/jesterschest.crt -noout -pubkey | sha256sum)
key_key=$(openssl pkey -in /etc/pki/tls/private/jesterschest.key -pubout | sha256sum)
[ "$cert_key" = "$key_key" ] || { echo "error: the certificate does not match the private key" >&2; exit 1; }
dnf -y -q install mariadb-connector-c > /dev/null
rpm -q libyubikey > /dev/null || { echo "error: install libyubikey from EPEL first" >&2; exit 1; }
# Rocky installs a minimal libcurl by default; the application needs SMTP.
if ! curl --version | grep -i '^protocols' | grep -qw smtp; then
    echo "error: this libcurl has no SMTP support; install the full one:" >&2
    echo "       sudo dnf swap libcurl-minimal libcurl" >&2
    exit 1
fi
[ -f "$CONF_DIR/mail.conf" ] || {
    echo "error: missing $CONF_DIR/mail.conf (see mail.conf.example and mail-pin.sh)" >&2; exit 1; }
grep -q "CHANGE-ME" "$CONF_DIR/mail.conf" && {
    echo "error: $CONF_DIR/mail.conf still has CHANGE-ME values" >&2; exit 1; }

step "Installing app.cgi and static files"
install -m 0755 -o root -g root "$HERE/app.cgi" /var/www/cgi-bin/app.cgi
install -d -m 0755 -o root -g root "$WEB_ROOT" "$WEB_ROOT/static"
install -m 0644 -o root -g root "$HERE"/static/* "$WEB_ROOT/static/"
restorecon -R /var/www/cgi-bin/app.cgi "$WEB_ROOT"

step "Application configuration in $CONF_DIR"
install -d -m 0750 -o root -g apache "$CONF_DIR"
if [ ! -f "$CONF_DIR/db.conf" ]; then
    read -r -s -p "Password of jc_app in MariaDB (not shown): " DB_PASS; echo
    umask 027
    cat > "$CONF_DIR/db.conf" << CONF
host = 192.168.161.20
port = 3306
user = jc_app
password = $DB_PASS
database = jesterschest
ssl_ca = $CONF_DIR/mariadb-ca.crt
CONF
    unset DB_PASS
fi
chown root:apache "$CONF_DIR/db.conf" && chmod 0640 "$CONF_DIR/db.conf"
chown root:apache "$CONF_DIR/mail.conf" && chmod 0640 "$CONF_DIR/mail.conf"
chown root:root "$CONF_DIR/mariadb-ca.crt" && chmod 0644 "$CONF_DIR/mariadb-ca.crt"

# Master key that seals the YubiKey secrets (AES-256-GCM). Created once and
# never replaced: the sealed keys in the database depend on it. Back it up
# OUTSIDE VMDatos (with the database backups it would defeat its purpose).
if [ ! -f "$CONF_DIR/yubikey-master.key" ]; then
    (umask 077 && openssl rand -hex 32 > "$CONF_DIR/yubikey-master.key")
    echo "Created $CONF_DIR/yubikey-master.key: back it up now, outside VMDatos."
fi
chown root:apache "$CONF_DIR/yubikey-master.key" && chmod 0640 "$CONF_DIR/yubikey-master.key"
restorecon -R "$CONF_DIR"

step "SELinux: CGI execution and database connections"
setsebool -P httpd_enable_cgi on
setsebool -P httpd_can_network_connect_db on
# Mail ports only (25, 465, 587), NOT httpd_can_network_connect (any port).
setsebool -P httpd_can_sendmail on

step "Apache site"
install -m 0644 -o root -g root "$HERE/jesterschest.conf" /etc/httpd/conf.d/jesterschest.conf
restorecon /etc/httpd/conf.d/jesterschest.conf
apachectl configtest
systemctl reload httpd

step "Done. Test from VMServicios with:"
echo "  curl -sk --resolve www.jesterschest.internal:443:192.168.160.10 https://www.jesterschest.internal/app.cgi/catalogo | grep summary"
