#!/usr/bin/env python3
"""Run outside a sandbox that forbids sockets. Real OpenSSL TLS; host parser harness, not ESP32 validation."""
import argparse
import datetime
import pathlib
import socket
import ssl
import subprocess
import tempfile
import threading

parser = argparse.ArgumentParser()
parser.add_argument("probe", help="compiled openssl_probe executable")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="lua-http-tls-") as tmp:
    root = pathlib.Path(tmp)
    def openssl(*argv):
        subprocess.run(["openssl", *argv], cwd=root, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", "ca.key", "-out", "ca.pem", "-days", "2", "-subj", "/CN=Lua HTTP fixture CA")
    openssl("req", "-new", "-newkey", "rsa:2048", "-nodes", "-keyout", "server.key", "-out", "server.csr", "-subj", "/CN=fixture.example")
    (root / "index").write_text("")
    (root / "serial").write_text("1000\n")
    (root / "ca.conf").write_text("""[ca]
default_ca=fixture
[fixture]
database=index
serial=serial
new_certs_dir=.
certificate=ca.pem
private_key=ca.key
default_md=sha256
policy=policy
x509_extensions=extensions
unique_subject=no
[policy]
commonName=supplied
[extensions]
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:fixture.example
""")
    now = datetime.datetime.now(datetime.timezone.utc)
    def stamp(days):
        return (now + datetime.timedelta(days=days)).strftime("%Y%m%d%H%M%SZ")
    for name, start, end in [("valid", -1, 1), ("expired", -3, -1), ("future", 1, 2)]:
        openssl("ca", "-batch", "-config", "ca.conf", "-in", "server.csr", "-out", f"{name}.pem", "-startdate", stamp(start), "-enddate", stamp(end))
    def case(name, host, expected, ca="ca.pem"):
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(root / f"{name}.pem", root / "server.key")
        seen = []
        listener = socket.socket()
        listener.bind(("127.0.0.1", 0)); listener.listen(1); listener.settimeout(10)
        port = listener.getsockname()[1]
        def serve():
            try:
                conn, _ = listener.accept()
                with conn:
                    with ctx.wrap_socket(conn, server_side=True) as tls:
                        # The client emits several TLS writes; one recv is not a complete request.
                        request = bytearray()
                        while b"\r\n\r\n" not in request:
                            chunk = tls.recv(4096)
                            if not chunk:
                                raise OSError("truncated fixture request")
                            request.extend(chunk)
                            if len(request) > 8192:
                                raise OSError("oversized fixture request")
                        seen.append(bytes(request))
                        tls.sendall(b"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello")
            except (ssl.SSLError, OSError):
                pass
            finally:
                listener.close()
        thread = threading.Thread(target=serve)
        thread.start()
        result = subprocess.run([args.probe, f"https://{host}/", "--fixture", str(port), str(root / ca)], capture_output=True)
        thread.join(15)
        assert not thread.is_alive(), "fixture server did not drain"
        assert (result.returncode == 0) == expected, result.stderr.decode()
        if expected:
            assert result.stdout == b"hello" and seen
        else:
            assert not seen, "HTTP bytes leaked before certificate validation"
        print(name, host, "PASS", result.stderr.decode().strip())
    case("valid", "fixture.example", True)
    case("valid", "wrong.example", False)
    case("expired", "fixture.example", False)
    case("future", "fixture.example", False)
    openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", "other.key", "-out", "other.pem", "-days", "2", "-subj", "/CN=Unrelated CA")
    case("valid", "fixture.example", False, "other.pem")
