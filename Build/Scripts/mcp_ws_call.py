#!/usr/bin/env python3
"""Minimal JSON-RPC client for UE_MCP_Bridge (ws://localhost:9877)."""
import argparse
import base64
import hashlib
import json
import os
import socket
import struct
import sys


def recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed")
        buf += chunk
    return buf


def handshake(sock, host, port):
    key = base64.b64encode(os.urandom(16)).decode()
    req = (
        f"GET / HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        f"Upgrade: websocket\r\n"
        f"Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        f"Sec-WebSocket-Version: 13\r\n"
        f"\r\n"
    )
    sock.sendall(req.encode())
    resp = b""
    while b"\r\n\r\n" not in resp:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("handshake failed")
        resp += chunk
    if b"101" not in resp.split(b"\r\n", 1)[0]:
        raise ConnectionError(f"handshake rejected: {resp[:200]!r}")
    expected = base64.b64encode(
        hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()
    )
    if expected not in resp:
        # Some servers skip accept validation on client; still require 101.
        pass


def send_text(sock, text):
    payload = text.encode("utf-8")
    n = len(payload)
    mask = os.urandom(4)
    header = bytearray()
    header.append(0x81)
    if n < 126:
        header.append(0x80 | n)
    elif n < 65536:
        header.append(0x80 | 126)
        header.extend(struct.pack(">H", n))
    else:
        header.append(0x80 | 127)
        header.extend(struct.pack(">Q", n))
    header.extend(mask)
    masked = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    sock.sendall(bytes(header) + masked)


def recv_text(sock):
    hdr = recv_exact(sock, 2)
    opcode = hdr[0] & 0x0F
    masked = hdr[1] & 0x80
    n = hdr[1] & 0x7F
    if n == 126:
        n = struct.unpack(">H", recv_exact(sock, 2))[0]
    elif n == 127:
        n = struct.unpack(">Q", recv_exact(sock, 8))[0]
    mask = recv_exact(sock, 4) if masked else b"\x00\x00\x00\x00"
    payload = recv_exact(sock, n)
    if masked:
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    if opcode == 0x8:
        raise ConnectionError("server closed")
    if opcode == 0x9:
        # ping -> pong
        return recv_text(sock)
    return payload.decode("utf-8")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("method")
    p.add_argument("--params", default="{}")
    p.add_argument("--params-file")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=9877)
    p.add_argument("--out")
    args = p.parse_args()
    if args.params_file:
        with open(args.params_file, encoding="utf-8") as f:
            params = json.load(f)
    else:
        params = json.loads(args.params)
    req = {"jsonrpc": "2.0", "id": 1, "method": args.method, "params": params}
    sock = socket.create_connection((args.host, args.port), timeout=120)
    try:
        handshake(sock, args.host, args.port)
        send_text(sock, json.dumps(req))
        sock.settimeout(120)
        raw = recv_text(sock)
    finally:
        sock.close()
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(raw)
        print(args.out)
    else:
        sys.stdout.write(raw)


if __name__ == "__main__":
    main()
