#!/usr/bin/env python3
"""Local SMTP server for development: behaves like the course server where it
matters (STARTTLS required, authentication required, self-signed
certificate) and prints every received message, decoded, so you can open the
verification links. Never used in production.

    pip install aiosmtpd --break-system-packages     (once)
    ./scripts/dev-smtp.py                            (leave it running)

scripts/dev-setup.sh creates the certificate and points mail.conf here.
"""
import email
import email.policy
import os
import ssl
import time

from aiosmtpd.controller import Controller
from aiosmtpd.smtp import AuthResult, LoginPassword

CERT_DIR = os.path.expanduser("~/.local/share/jesterschest-dev-smtp")
HOST, PORT = "127.0.0.1", 2525
USER, PASSWORD = b"tienda@jesterschest.internal", b"dev-password"


class PrintHandler:
    async def handle_DATA(self, server, session, envelope):
        message = email.message_from_bytes(envelope.content, policy=email.policy.default)
        print("=" * 72)
        print(f"Para: {', '.join(envelope.rcpt_tos)}   De: {envelope.mail_from}")
        print(f"Asunto: {message['subject']}")
        print("-" * 72)
        print(message.get_content().strip())
        print("=" * 72, flush=True)
        return "250 OK"


def authenticator(server, session, envelope, mechanism, auth_data):
    ok = isinstance(auth_data, LoginPassword) and auth_data.login == USER and auth_data.password == PASSWORD
    return AuthResult(success=ok)


def main():
    context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
    context.load_cert_chain(os.path.join(CERT_DIR, "server.crt"), os.path.join(CERT_DIR, "server.key"))
    controller = Controller(PrintHandler(), hostname=HOST, port=PORT, tls_context=context,
                            require_starttls=True, authenticator=authenticator,
                            auth_require_tls=True, auth_required=True)
    controller.start()
    print(f"SMTP de desarrollo en {HOST}:{PORT} (STARTTLS y autenticación obligatorios). Ctrl+C para salir.", flush=True)
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        controller.stop()


if __name__ == "__main__":
    main()
