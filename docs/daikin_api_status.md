# Daikin local API — status and library notes

## My hardware

| Item                          | Value                 |
| ----------------------------- | --------------------- |
| Indoor unit model             | FTXM20N2V1B (2020/06) |
| WiFi adapter                  | BRP069B41             |
| Firmware (`ver`)              | 1.14.68               |
| Hardware rev (`rev`)          | C3FF8A6               |
| Adapter API mode (`adp_mode`) | `run`                 |

Identified via:

```bash
curl -s http://192.168.1.73/common/basic_info
# ret=OK,type=aircon,reg=eu,dst=1,ver=1_14_68,rev=C3FF8A6,...
```

The BRPxxx model number is **not** exposed over HTTP — only firmware/region. The
model string is printed on a sticker on the WiFi PCB inside the indoor unit's
front panel.

## State of the local API across Daikin firmwares

Daikin progressively locked down the local HTTP API on newer adapters. Three regimes exist:

| Firmware regime              | Adapters                                       | Local API                                                                                                          | Library support                                                                                       |
| ---------------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------- |
| **Legacy REST**              | BRP069A/B, early C4x, BRP072, BRP15B61 AirBase | `/common/basic_info`, `/aircon/get_sensor_info`, `/aircon/get_control_info` returning CSV `ret=OK,type=aircon,...` | ✅ Works with `daikinapi` and `pydaikin`                                                              |
| **Locked-down gap (~2.0.0)** | BRP069C4x firmware 2.0.0                       | Legacy endpoints return stub `{"rsc":2000,"method":"polling",...}`; new JSON API not yet present                   | ❌ Neither library works; adapter pushed toward ONECTA cloud polling. Fix = update firmware to 2.8.0+ |
| **New JSON API (≥2.8.0)**    | BRP069C4x, BRP084Cxx                           | `POST /dsiot/multireq` with JSON body                                                                              | ✅ `pydaikin` only (HA core ≥ 2025.9)                                                                 |

Quick check commands:

```bash
# legacy CSV API
curl -s http://<ip>/common/basic_info

# new dsiot JSON API (2.8.0+)
curl -s -X POST http://<ip>/dsiot/multireq \
  -H 'Content-Type: application/json' \
  -d '{"requests":[{"op":2,"to":"/dsiot/edge/adr_0100.dgc_status"}]}'
```

**Verdict for my setup:** BRP069B41 + firmware 1.14.68 → legacy REST API.
Both `daikinapi` and `pydaikin` work today. No firmware concerns.

## Python libraries

### `daikinapi` (currently used)

- Repo: <https://github.com/arska/python-daikinapi> — **archived 2023-12-13**
- Last release: v1.0.8, 2023-02-03
- Sync, requests-based, BRP069 legacy CSV API only.
- Known issue: `target_temperature`, `target_humidity` properties do
  `float(self._get_control()["stemp"])` unconditionally. When the device returns
  `'--'` (value unavailable), they raise `ValueError` — visible in our logs as:

  ```
  Error when getting value (target_temperature): could not convert string to float: '--'. Ignored.
  Error when getting value (target_humidity): could not convert string to float: '--'. Ignored.
  ```

  The fix can't be applied via the existing `UNAVAILABLE_SENTINELS` filter in
  `daikin_to_influx_v2.py` because the exception is raised inside the property
  before the string reaches our code.

### `pydaikin` (recommended replacement)

- Repo: <https://github.com/fredrike/pydaikin>
- PyPI: <https://pypi.org/project/pydaikin/>
- Active — v2.18.1 released 2026-02-25.
- Async (`aiohttp`), GPL-3.0, requires Python ≥ 3.12.
- Used by Home Assistant's official Daikin integration — tracks firmware quirks.
- Supports: BRP069Axx/Bxx, BRP072Axx, BRP072B/Cxx, **BRP084 / BRP069C4x ≥ 2.8.0**
  (`/dsiot/multireq` auto-detected), BRP15B61 AirBase, SKYFi.
- Explicitly **not** supported: BRP069C4x firmware 2.0.0 (the gap above).

API shape:

```python
from pydaikin.factory import DaikinFactory

device = await DaikinFactory("192.168.1.73")
await device.update_status()
device.values["htemp"]   # inside temperature (str)
device.values["otemp"]   # outside temperature
device.values["stemp"]   # target temperature ('--' when unavailable)
device.values["shum"]    # target humidity
```

Unavailable values are returned as the raw `'--'` string — our existing
`UNAVAILABLE_SENTINELS` filter would work as intended.

### Other candidates (not applicable)

- `python-daikin-altherma` — scoped to Altherma heat pumps, different
  endpoints, not for split AC units.
- `daimik/Daikin-BRP069C4x-Local-API` — reverse-engineered endpoint reference
  documentation, not a library. Useful if calling endpoints directly.

## Migration notes for `daikin_to_influx_v2.py`

If/when moving to `pydaikin`:

1. **Sync → async**: wrap polls with `asyncio.run()` or convert `main()` to `async def`.
2. **Attribute → dict access**: replace `API.target_temperature` with
   `device.values["stemp"]` after `await device.update_status()`. Raw string
   values; cast in our code.
3. **Field name mapping**: build a small `{nice_name: raw_key}` dict.
   - `target_temperature` → `stemp`
   - `target_humidity` → `shum`
   - `inside_temperature` → `htemp`
   - `outside_temperature` → `otemp`
   - `power` → `pow`
   - `mode` → `mode`
   - `fan_rate` → `f_rate`
   - `fan_direction` → `f_dir`
   - `compressor_frequency` → `cmpfreq`
4. **Logged `'--'` errors disappear**: pydaikin returns the raw string, so the
   existing `UNAVAILABLE_SENTINELS` check at
   [miniha/daikin_to_influx_v2.py:55](../miniha/daikin_to_influx_v2.py#L55)
   filters them silently.
5. **Future-proof**: if Daikin pushes a firmware update that switches our
   BRP069B41 to the new API (unlikely on B-series, but possible), pydaikin
   handles it; `daikinapi` would break silently.

## References

- pydaikin: <https://github.com/fredrike/pydaikin>
- pydaikin issue #83 (BRP069C4x firmware 2.0.0 stub response): <https://github.com/fredrike/pydaikin/issues/83>
- HA core issue #99251 (firmware 2.8.0 dsiot API switch): <https://github.com/home-assistant/core/issues/99251>
- HA core issue #121450 (BRP069C4x support): <https://github.com/home-assistant/core/issues/121450>
- Home Assistant Daikin integration docs: <https://www.home-assistant.io/integrations/daikin/>
- BRP069C4x endpoint reference: <https://github.com/daimik/Daikin-BRP069C4x-Local-API>
- Archived `daikinapi`: <https://github.com/arska/python-daikinapi>
