"""Fetch mesh credentials and device list from your AwoX account (run once).

Usage:  pip install requests  &&  python get_awox_mesh.py
Writes awox_raw.json (gitignored) – it contains secrets, don't share it.
"""
import getpass
import json
import uuid

import requests

URLS = [
    "https://l4hparse-prod.awox.cloud/parse/",     # AwoX Smart Control
    "https://l4hparse-hc-prod.awox.cloud/parse/",  # AwoX HomeControl / EGLO
]
APP_ID = "55O69FLtoxPt67LLwaHGpHmVWndhZGn9Wty8PLrJ"
CLIENT_KEY = "PyR3yV65rytEicteNlQHSVNpAGvCByOrsLiEqJtI"

user = input("AwoX e-mail: ")
pw = getpass.getpass("AwoX password: ")

headers = {
    "x-parse-application-id": APP_ID,
    "x-parse-installation-id": str(uuid.uuid4()),
    "x-parse-client-key": CLIENT_KEY,
    "content-type": "application/json",
}

api, login, r = None, None, None
for url in URLS:
    r = requests.post(url + "login", headers=headers,
                      data=json.dumps({"username": user, "password": pw, "_method": "GET"}))
    if r.status_code == 200:
        api, login = url, r.json()
        break
if not api:
    raise SystemExit(f"Login failed: {r.text}")
print(f"Login OK ({api})\n")

headers["x-parse-session-token"] = login["sessionToken"]
query = json.dumps({
    "where": {"owner": {"__type": "Pointer", "className": "_User", "objectId": login["objectId"]}},
    "_method": "GET",
})


def fetch(cls):
    resp = requests.post(api + "classes/" + cls, headers=headers, data=query)
    resp.raise_for_status()
    return resp.json()["results"]


creds = fetch("Credential")
devices = fetch("Device")

print("=== CREDENTIALS ===")
for c in creds:
    print(f"{c.get('service')}: client_id={c.get('client_id')}  "
          f"access_token={c.get('access_token')}  username={c.get('username')}")

print("\n=== DEVICES ===")
for d in devices:
    print(f"{d.get('displayName')}: MAC={d.get('macAddress')}  "
          f"Mesh-ID={d.get('address')}  Type={d.get('type')}")

with open("awox_raw.json", "w", encoding="utf-8") as f:
    json.dump({"credentials": creds, "devices": devices}, f, indent=2)
print("\nRaw data saved to awox_raw.json")

# Ready-to-paste lines for secrets.h
by_service = {c.get("service"): c for c in creds}
mesh = by_service.get("zigbee") or by_service.get("mesh") or {}
print("\n=== COPY INTO secrets.h ===")
print('#define MESH_NAME_S       "%s"' % mesh.get("client_id"))
print('#define MESH_PASS_S       "%s"' % mesh.get("access_token"))
for d in devices:
    print("// %s:" % d.get("displayName"))
    print('#define LAMP_MAC_S        "%s"' % str(d.get("macAddress")).lower())
    print("#define MESH_ID_S         %s" % d.get("address"))
