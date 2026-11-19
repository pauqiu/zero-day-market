#!/usr/bin/env bash
# Shows the identity of the mail server and the pin for mail.conf.
# Run it on VMServicios (the machine that will send the mail):
#   bash mail-pin.sh [host] [port]
#
# Trust on first use: the key is obtained over the network, so compare the
# SHA-256 fingerprint with the one a classmate gets from another machine, or
# with the one the professor publishes, before copying the pin into mail.conf.
set -euo pipefail
HOST="${1:-172.24.131.105}"
PORT="${2:-587}"

CERT=$(openssl s_client -connect "$HOST:$PORT" -starttls smtp </dev/null 2>/dev/null \
       | openssl x509) || { echo "error: no certificate from $HOST:$PORT" >&2; exit 1; }

echo "Certificado del servidor $HOST:$PORT"
echo "$CERT" | openssl x509 -noout -subject -issuer -dates
echo "$CERT" | openssl x509 -noout -fingerprint -sha256
echo
echo "Línea para mail.conf (si la huella coincide con la de otra fuente):"
echo "pinned_public_key = sha256//$(echo "$CERT" | openssl x509 -noout -pubkey \
    | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | openssl base64)"
