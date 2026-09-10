# USB-independent remote DMOTA2 tool

`scripts/dmota_remote.py` replaces USB-gated scratch deployment helpers for
future remote operations. It does not change firmware or the legacy DMOTA1
controller. Use only a trusted LAN or VPN: HTTP Digest is request authentication,
not encryption or cryptographic hardware attestation. Do not expose port 80 to
the public Internet. The device ID is checked in the authenticated status API;
there is no USB enumeration or mandatory mDNS lookup.

Read-only preflight for the last observed development image (313):

```sh
python3 scripts/dmota_remote.py preflight \
  --url http://192.168.1.55 --device dm-bridge-8810a1 \
  --credentials .arduino/evidence/hardware/private/b0-usb-20260908-tk5fyupp/credentials.txt \
  --public-key 8907a642038be300de63e91c31c4313d06bff22d1e0767f83eeea54bde26f1b1 \
  --current-version 313 --current-build usb-bootstrap-0.3.13-dev \
  --config-schema 1 --config-revision 1
```

Use the actual VPN-reachable IP/origin after relocation. Credentials are read
from an owner-only file, never passed as a secret command-line argument. The
tool pins device, board, public key ID, current build/version and configuration;
it requires STA, healthy VALID/IDLE, TX blocked, and GitHub automatic mode OFF
with no active job. A mismatch stops before prepare. It never disables automatic
mode on your behalf. This is intentionally a diagnostic-firmware safety profile,
not a tool for a future TX-enabled production build.

For an explicitly approved deployment, use the same arguments with `deploy`
instead of `preflight`, and add `--package /absolute/path/to/new.dmota`.
First run `preflight --package ...` to verify the signed candidate without writes.
The candidate must be newer than the pinned current version and compatible with
the configuration schema. Exactly one file-prepare and one image upload are
attempted. Reboot/response timeouts cause GET-only reconciliation, never resend
or an external confirmation POST. Success requires the candidate build/version,
matching transaction and all readiness checks at VALID.

An owner-only per-device fence is created before prepare under
`.arduino/evidence/hardware/private/dmota-remote/`. Keep this directory consistent
across invocations; coordinate with other operators/controllers separately.
The fence intentionally blocks subsequent deployments, including other versions.
After an interrupted transfer, run `reconcile` with the **original current-image
arguments** and the same origin/state directory. It performs GETs only and retains
the fence. If prepare's response was lost, no transaction is available: inspect
the device and fence manually, do not repeat prepare/upload blindly. After a
confirmed VALID result, archive the fence manually before a separately approved
next deployment. Never delete an uncertain fence or select a new state directory
to bypass it. No automatic retries, background service, GitHub Release publication,
USB recovery, Wi-Fi reset, or automatic-update configuration is performed.

Host tests and a network preflight do not prove a field transfer, field power
stability, or RS-485 behavior. The previously reported Tail485 web/TX/RX operation
is user-reported evidence for that earlier setup, not a current 313 TX test.
