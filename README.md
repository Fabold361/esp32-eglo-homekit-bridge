# BlueKit

ESP32 bridge that adds an AwoX/EGLO Connect-Z Bluetooth lamp to Apple Home. Uses [HomeSpan](https://github.com/HomeSpan/HomeSpan) for HomeKit and [NimBLE](https://github.com/h2zero/NimBLE-Arduino) to talk to the lamp's encrypted Telink mesh protocol. Supports on/off and brightness via the Home app, Control Center and Siri.

```
iPhone / Home app ──WiFi──▶ ESP32 (HomeKit accessory) ──Bluetooth──▶ EGLO lamp
```

No hub, no cloud, no Homebridge. The iPhone only needs to be on the same WiFi. The ESP32 has to be near the lamp (same room works best).

## Features

- On/off and brightness (1–100 %) in Apple Home
- Control Center tile: tap = on/off, long-press = brightness slider
- Siri, scenes, automations, Action Button (iPhone 15 Pro and newer)
- Your own HomeKit setup code

## What you need

- An ESP32 with Bluetooth (tested: ESP32-WROOM-32) and a USB cable
- An AwoX/EGLO Connect-Z lamp that's already set up in the **AwoX HomeControl** / **EGLO Connect** app
- A PC with [Arduino IDE 2](https://www.arduino.cc/en/software) and [Python 3](https://www.python.org/downloads/)
- 2.4 GHz WiFi

---

## Step 1 – Download the project

Click **Code → Download ZIP** on GitHub and unpack it, or use:

```bash
git clone https://github.com/Fabold361/esp32-eglo-homekit-bridge.git
```

## Step 2 – Get your lamp data

The lamp only accepts commands after an encrypted login. The login data is stored in your AwoX/EGLO account, so a small script fetches it.

1. Open a terminal in the `tools` folder (Windows: Shift + right-click in the folder → *Open in Terminal*).
2. Install the library once:
   ```bash
   pip install requests
   ```
3. Run the script:
   ```bash
   python get_awox_mesh.py
   ```
   On Windows `py get_awox_mesh.py` also works.
4. Enter the e-mail and password of your AwoX/EGLO app account. The password stays invisible while you type, that's normal.

At the end the script prints a block like this:

```
=== COPY INTO secrets.h ===
#define MESH_NAME_S       "AbCd1234"
#define MESH_PASS_S       "XyZ98765"
// Living room lamp:
#define LAMP_MAC_S        "aa:bb:cc:dd:ee:ff"
#define MESH_ID_S         1234
```

If you have more than one lamp, pick the lines under the name of the lamp you want.

> ⚠️ The script also saves `awox_raw.json`. It contains your account secrets. Don't share it and don't upload it. It's in `.gitignore`.

## Step 3 – Create `secrets.h`

1. In the `BlueKit` folder, copy `secrets.example.h` and name the copy **`secrets.h`**.
2. Open `secrets.h` and fill in your values:

| Line | What to enter |
|------|---------------|
| `WIFI_SSID_S` | Name of your WiFi (2.4 GHz) |
| `WIFI_PASS_S` | WiFi password |
| `LAMP_MAC_S` | From the script (lowercase, with `:`) |
| `MESH_NAME_S` | From the script |
| `MESH_PASS_S` | From the script |
| `MESH_ID_S` | From the script (number, no quotes) |
| `ACCESSORY_NAME_S` | Name the lamp gets in Apple Home, e.g. `"Desk Lamp"` |
| `PAIRING_CODE_S` | Your own 8-digit setup code, e.g. `"20481337"` → in the Home app you type `204-81-337` |

Rules for the setup code: exactly 8 digits, no dashes. Simple codes like `12345678`, `87654321` or `11111111` are rejected by Apple.

`secrets.h` is in `.gitignore`, so it never gets uploaded to GitHub.

## Step 4 – Set up Arduino IDE

1. **Board package:** *Tools → Board → Boards Manager* → search for **esp32** (by Espressif) → install version **3.3.0 or newer**.
2. **Libraries:** *Tools → Manage Libraries* → install **HomeSpan** and **NimBLE-Arduino**.
3. Open `BlueKit/BlueKit.ino`. The folder must stay named `BlueKit`, otherwise the IDE complains.
4. Under *Tools*, set:
   - Board: **ESP32 Dev Module**
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)** (the sketch is ~1.7 MB and doesn't fit otherwise)
   - Port: the COM port of your ESP32

## Step 5 – Upload

1. Click **Upload** (→). The first build takes a few minutes.
2. When you see `Hard resetting via RTS pin...`, the upload is done.
3. Open the **Serial Monitor** and set it to **115200 baud**. You should see HomeSpan start and connect to your WiFi.

## Step 6 – Add to Apple Home

1. Home app → **+** → **Add Accessory** → **More options…**
2. Pick **BlueKit Lamp**.
3. Confirm "Uncertified Accessory" with **Add Anyway**.
4. Enter your setup code from `PAIRING_CODE_S`.
5. Pick a room. Done ✅

### Optional: Control Center and Action Button

- **Control Center:** Swipe down → long-press empty space → *Add a Control* → **Home** → choose the lamp.
- **Action Button:** Settings → Action Button → **Controls** → Home → choose the lamp.
- **Use while locked:** Settings → Face ID & Passcode → enable **Home Control**. Anyone holding your phone can then switch the lamp, but that's all they can do.

---

## How it works

1. A HomeKit change arrives. `update()` only remembers it and answers right away, so the Home app doesn't hang.
2. `loop()` connects to the lamp over Bluetooth. The lamp allows only one connection at a time.
3. Telink mesh login: the ESP32 sends `0x0C + random + AES(name ⊕ pass)`. The lamp answers `0x0D` + its own random. Both sides compute the session key from that.
4. The command goes out as a 17-byte Connect-Z frame: CRC-8 + length + mesh ID + command, AES-encrypted.
5. The ESP32 disconnects, so the original app can still reach the lamp.

| Command    | Bytes |
|------------|-------|
| On         | `01 06 00 01` |
| Off        | `01 06 00 00` |
| Brightness | `01 08 00 04 <v> 02 00 00 00 00` (v = 3…254) |

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Random characters in Serial Monitor | Set the baud rate to **115200** |
| "Sketch too big" | Partition Scheme → **Huge APP** |
| `secrets.h: No such file` | You haven't done Step 3 yet |
| WiFi won't connect | Use a 2.4 GHz network and check the name and password (they're case-sensitive) |
| `Lamp not reachable` | Close the AwoX/EGLO app (it holds the Bluetooth connection), move the ESP32 closer, check the MAC |
| Lamp connects but ignores commands | Wrong `MESH_PASS_S` or `MESH_ID_S`. Use the `access_token` from the **zigbee** credential |
| Script: `Login failed` | Wrong e-mail or password of the app account |
| Accessory doesn't show up in the Home app | iPhone and ESP32 must be on the same WiFi. Unplug and replug the ESP32 |
| Want to pair again | Remove the accessory in the Home app, type `U` in the Serial Monitor (unpair), then add it again |

## Credits

- [python-awox-mesh-light](https://github.com/Leiaz/python-awox-mesh-light) by Leiaz – Telink mesh login and crypto
- [awoxble2mqtt-api](https://github.com/BastiAKA/awoxble2mqtt-api) by BastiAKA – Connect-Z command format
- [HomeSpan](https://github.com/HomeSpan/HomeSpan) – HomeKit for ESP32

Not affiliated with AwoX, EGLO or Apple.

## License

[MIT](LICENSE)
