#!/usr/bin/env python3
"""Push a firmware image to glasses running Debug > Wireless Flash.

Usage: python3 scripts/ota_push.py <host[:port]> <path/to/embedded-app.bin>
"""

import argparse
import hashlib
import http.client
import json
import os
import sys

CHUNK_SIZE = 16 * 1024


def split_host(host):
    if ":" in host:
        name, port = host.rsplit(":", 1)
        return name, int(port)
    return host, 80


def get_info(name, port):
    conn = http.client.HTTPConnection(name, port, timeout=5)
    try:
        conn.request("GET", "/info")
        resp = conn.getresponse()
        if resp.status != 200:
            return None
        return json.loads(resp.read())
    finally:
        conn.close()


def push(name, port, image_path):
    size = os.path.getsize(image_path)
    with open(image_path, "rb") as f:
        md5 = hashlib.md5(f.read()).hexdigest()

    conn = http.client.HTTPConnection(name, port, timeout=60)
    conn.putrequest("POST", "/flash")
    conn.putheader("Content-Type", "application/octet-stream")
    conn.putheader("Content-Length", str(size))
    conn.putheader("X-MD5", md5)
    conn.endheaders()

    sent = 0
    with open(image_path, "rb") as f:
        while chunk := f.read(CHUNK_SIZE):
            conn.send(chunk)
            sent += len(chunk)
            pct = sent * 100 // size
            print(f"\rUploading {sent // 1024}/{size // 1024} KiB ({pct}%)", end="", flush=True)
    print()

    resp = conn.getresponse()
    body = resp.read().decode(errors="replace").strip()
    conn.close()
    return resp.status, body


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="IP/hostname shown on the glasses, optionally host:port")
    parser.add_argument("image", help="Firmware image, e.g. build-glasses-debug/embedded-app.bin")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        sys.exit(f"Firmware image not found: {args.image}")

    name, port = split_host(args.host)
    try:
        info = get_info(name, port)
    except OSError as e:
        sys.exit(
            f"Could not reach {name}:{port} ({e}).\n"
            "Open Debug > Wireless Flash on the glasses and check they're on the same network."
        )
    if info:
        print(f"Glasses running {info.get('version')} from {info.get('partition')}")

    try:
        status, body = push(name, port, args.image)
    except OSError as e:
        sys.exit(f"\nUpload failed: {e}")

    if status != 200:
        sys.exit(f"Flash failed (HTTP {status}): {body}")
    print(body)


if __name__ == "__main__":
    main()
