#!/usr/bin/env python3
"""Push a firmware image to glasses over WiFi (requires CONFIG_DEV_WIRELESS_FLASH).

Without --host, the glasses are found over BLE and asked to start listening (BLE needs `bleak`).
With --host, the image goes straight to glasses showing Debug > Wireless Flash.
Images are signed with the per-checkout .ota-key that was baked into the running firmware.
"""

import argparse
import asyncio
import hashlib
import hmac
import http.client
import json
import os
import sys
import time

CHUNK_SIZE = 16 * 1024
SOL_SERVICE_UUID = "db65598c-04b1-469f-833f-517f285db256"
SETTINGS_CHARACTERISTIC_UUID = "611f68af-5ffe-4944-9b8f-ef6773326268"
DEFAULT_KEY_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".ota-key")


def sign(key, data):
    return hmac.new(key.encode(), data, hashlib.sha256).hexdigest()


def split_host(host):
    if ":" in host:
        name, port = host.rsplit(":", 1)
        return name, int(port)
    return host, 80


async def ble_request(client, queue, payload, prefix, timeout):
    await client.write_gatt_char(SETTINGS_CHARACTERISTIC_UUID, payload.encode(), response=True)
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError(f"No '{prefix}' response from glasses")
        message = await asyncio.wait_for(queue.get(), timeout=remaining)
        if message.startswith(prefix + "="):
            return message[len(prefix) + 1 :]


async def ble_trigger(key, device_filter):
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError:
        sys.exit(
            "BLE trigger needs bleak: "
            f"{sys.executable} -m pip install bleak\n(or pass --host to push directly)"
        )

    print("Scanning for glasses over BLE...")
    found = await BleakScanner.discover(timeout=8, service_uuids=[SOL_SERVICE_UUID], return_adv=True)
    candidates = list(found.values())
    if device_filter:
        needle = device_filter.lower()
        candidates = [
            (d, adv)
            for d, adv in candidates
            if needle in (d.name or "").lower() or needle in d.address.lower()
        ]
    if not candidates:
        sys.exit("No glasses found over BLE. Are they awake and nearby?")
    candidates.sort(key=lambda c: c[1].rssi, reverse=True)
    if len(candidates) > 1:
        print("Found multiple glasses (pick one with --device):")
        for d, adv in candidates:
            print(f"  {d.address}  {d.name}  RSSI {adv.rssi}")
    device = candidates[0][0]
    print(f"Connecting to {device.name} ({device.address})")

    queue = asyncio.Queue()
    async with BleakClient(device) as client:
        await client.start_notify(
            SETTINGS_CHARACTERISTIC_UUID,
            lambda _, data: queue.put_nowait(bytes(data).decode(errors="replace")),
        )
        nonce = await ble_request(client, queue, "wireless-flash-nonce", "wireless-flash-nonce", 10)
        response = sign(key, f"wireless-flash:{nonce}".encode())
        print("Waiting for glasses to join WiFi...")
        result = await ble_request(client, queue, f"wireless-flash={response}", "wireless-flash", 90)

    if result == "error:auth":
        sys.exit("Glasses rejected the BLE challenge. Was this build flashed from this checkout's .ota-key?")
    if result.startswith("error:"):
        sys.exit(f"Glasses couldn't start wireless flash: {result[len('error:'):]}")
    return result


def get_info(name, port):
    conn = http.client.HTTPConnection(name, port, timeout=5)
    try:
        conn.request("GET", "/info")
        resp = conn.getresponse()
        return json.loads(resp.read()) if resp.status == 200 else None
    finally:
        conn.close()


def wait_for_info(name, port, attempts):
    for attempt in range(attempts):
        try:
            return get_info(name, port)
        except OSError as e:
            if attempt == attempts - 1:
                sys.exit(
                    f"Could not reach {name}:{port} ({e}).\n"
                    "Check the glasses and this computer are on the same network."
                )
            time.sleep(1)


def push(name, port, image, signature):
    md5 = hashlib.md5(image).hexdigest()
    conn = http.client.HTTPConnection(name, port, timeout=60)
    conn.putrequest("POST", "/flash")
    conn.putheader("Content-Type", "application/octet-stream")
    conn.putheader("Content-Length", str(len(image)))
    conn.putheader("X-MD5", md5)
    conn.putheader("X-Signature", signature)
    conn.endheaders()

    size = len(image)
    for offset in range(0, size, CHUNK_SIZE):
        conn.send(image[offset : offset + CHUNK_SIZE])
        sent = min(offset + CHUNK_SIZE, size)
        print(f"\rUploading {sent // 1024}/{size // 1024} KiB ({sent * 100 // size}%)", end="", flush=True)
    print()

    resp = conn.getresponse()
    body = resp.read().decode(errors="replace").strip()
    conn.close()
    return resp.status, body


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("image", help="Firmware image, e.g. build-glasses-debug/embedded-app.bin")
    parser.add_argument("--host", help="Skip BLE and push to this IP/hostname[:port]")
    parser.add_argument("--device", help="BLE name/address substring when several glasses are nearby")
    parser.add_argument("--key", default=DEFAULT_KEY_FILE, help="Signing key file (default: .ota-key)")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        sys.exit(f"Firmware image not found: {args.image}")
    if not os.path.isfile(args.key):
        sys.exit(f"Signing key not found: {args.key} (it's created by the first glasses-debug build)")
    with open(args.key) as f:
        key = f.read().strip()
    with open(args.image, "rb") as f:
        image = f.read()

    host = args.host or asyncio.run(ble_trigger(key, args.device))
    name, port = split_host(host)
    info = wait_for_info(name, port, attempts=1 if args.host else 10)
    if info:
        print(f"Glasses at {name}:{port} running {info.get('version')} from {info.get('partition')}")

    try:
        status, body = push(name, port, image, sign(key, image))
    except OSError as e:
        sys.exit(f"\nUpload failed: {e}")

    if status != 200:
        sys.exit(f"Flash failed (HTTP {status}): {body}")
    print(body)


if __name__ == "__main__":
    main()
