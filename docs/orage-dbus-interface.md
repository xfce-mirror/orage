# Orage DBus Interface

This document describes the public DBus interface provided by Orage. The
interface allows external applications and scripts to control Orage and manage
calendar files.

The interface is intended for **automation and integration**, not for
high-frequency or real-time control.

---

## Service Overview

- **Bus**: Session bus
- **Service name**: `org.xfce.orage`
- **Object path**: `/org/xfce/orage`
- **Interface**: `org.xfce.orage`

The service is registered when Orage is running. All methods are synchronous
and return no values on success. Errors are reported using standard DBus error
responses.

---

## Common Conventions

- All file paths are expected to be **absolute paths**.
- All strings are UTF-8 encoded.
- Methods return `void` on success.
- On failure, a `G_DBUS_ERROR` is returned.

---

## Methods

### LoadFile

**Name**: `LoadFile`

Load a calendar file into Orage.

This method loads the given calendar file and optionally assigns it to a
specific destination calendar.

**Parameters**
- `filename` (`s`): Path to the calendar file to load.
- `calendar_name` (`s`): Optional destination calendar name.
  - empty → default calendar (`orage.ics`)
  - Any other value → external calendar

**Errors**
- `org.freedesktop.DBus.Error.FileNotFound`

**Example: Load a calendar file into Orage**
```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.LoadFile \
  "/path/to/calendar.ics" \
  ""
```

---

### OpenFile

**Name**: `OpenFile`

Open a calendar file in Orage import dialog.

This method opens the specified file and makes it visible in the import dialog.

**Parameters**
- `filename` (`s`): Path to the calendar file.

**Errors**
- `org.freedesktop.DBus.Error.FileNotFound`

**Example: Open calendar file with import dialog**
```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.OpenFile \
  "/path/to/calendar.ics"
```

---

### ImportFile

**Name**: `ImportFile`

Import appointments from a calendar file into Orage.

The contents of the file are imported into the default calendar.

**Parameters**
- `filename` (`s`): Path to the calendar file.

**Errors**
- `org.freedesktop.DBus.Error.FileNotFound`

**Example: Import from file**
```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.ImportFile \
  "/path/to/input.ics"
```

---

### ExportFile

**Name**: `ExportFile`

Export appointments from Orage to a calendar file.

If no UIDs are specified, all appointments are exported.

**Parameters**
- `filename` (`s`): Destination file path.
- `uids` (`s`): Optional list of appointment UIDs.
  - Empty or NULL → export all appointments

**Errors**
- `org.freedesktop.DBus.Error.FileNotFound`

**Example: Export to file**
```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.ExportFile \
  "/path/to/output.ics" \
  ""

---

### AddForeign

**Name**: `AddForeign`

Add a foreign (external) calendar file to Orage.

The calendar can be added as read-only or read-write.

**Parameters**
- `filename` (`s`): Path to the external calendar file.
- `display_name` (`s`): Human-readable name shown in the UI.
- `read_only` (`b`): Set to `true` to add the calendar as read-only.

**Errors**
- `org.freedesktop.DBus.Error.FileNotFound`

**Example: Add a foreign calendar (read-only)**

```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.AddForeign \
  "/path/to/external.ics" \
  "External Calendar" \
  true
```

---

### RemoveForeign

**Name**: `RemoveForeign`

Remove a previously added foreign calendar file.

**Parameters**
- `filename` (`s`): Path to the foreign calendar file.

**Errors**
- `org.freedesktop.DBus.Error.FileNotFound`

**Example: Remove a foreign calendar**

```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.RemoveForeign \
  "/path/to/external.ics"
```

---

### OpenDay

**Name**: `OpenDay`

Open Orage focused on a specific date.

The date string supports multiple formats.

**Parameters**
- `date` (`s`): Date or date-time string.

**Supported formats**
- `YYYY-MM-DD`
- `YYYY-MM-DD HH:MM:SS` (local time)
- ISO-8601 date-time string

**Errors**
- `org.freedesktop.DBus.Error.InvalidArgs`

**Example: Open specific day**
Using `gdbus` to open Orage on a specific day:

```sh
gdbus call --session \
  --dest org.xfce.orage \
  --object-path /org/xfce/orage \
  --method org.xfce.orage.OpenDay \
  "2026-01-22"
```

---

## Error Handling

All methods return DBus errors on failure. Common errors include:

- `org.freedesktop.DBus.Error.FileNotFound`
- `org.freedesktop.DBus.Error.InvalidArgs`

Error messages are intended for diagnostics and may change.

---

## Stability Notes

This DBus interface is considered **stable but not frozen**. New methods may be
added in future releases, but existing methods and parameters should remain
backward compatible.

---
