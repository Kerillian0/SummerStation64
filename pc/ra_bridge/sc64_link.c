/**
 * @file sc64_link.c
 * @brief Talking to the SummerCart64 over USB from Windows (see sc64_link.h).
 */

#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

#include "sc64_link.h"

static HANDLE port = INVALID_HANDLE_VALUE;
static char port_name[32];
static char error_text[256];

static void fail (const char *text) {
    snprintf(error_text, sizeof(error_text), "%s (Windows error %lu)", text, (unsigned long) GetLastError());
}

const char *sc64_error (void) {
    return error_text;
}

const char *sc64_port (void) {
    return port_name;
}

/* The port whose name or hardware ID says SummerCart64 (or its FTDI chip, VID 0403 PID 6014). */
static bool find_port (char *out, size_t size) {
    HDEVINFO devices = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool found = false;
    SP_DEVINFO_DATA info = { .cbSize = sizeof(SP_DEVINFO_DATA) };
    for (DWORD i = 0; !found && SetupDiEnumDeviceInfo(devices, i, &info); i++) {
        char name[256] = "", hardware[256] = "";
        SetupDiGetDeviceRegistryPropertyA(devices, &info, SPDRP_FRIENDLYNAME, NULL, (BYTE *) name, sizeof(name), NULL);
        SetupDiGetDeviceRegistryPropertyA(devices, &info, SPDRP_HARDWAREID, NULL, (BYTE *) hardware, sizeof(hardware), NULL);
        bool ours = strstr(name, "SC64") || strstr(name, "SummerCart") || strstr(hardware, "VID_0403&PID_6014");
        char *com = strstr(name, "(COM");
        if (ours && com) {
            snprintf(out, size, "%s", com + 1);
            char *end = strchr(out, ')');
            if (end) {
                *end = '\0';
            }
            found = true;
        }
    }
    SetupDiDestroyDeviceInfoList(devices);
    return found;
}

static bool dsr_is (bool high) {
    for (int waited = 0; waited < 2000; waited++) {
        DWORD modem = 0;
        if (!GetCommModemStatus(port, &modem)) {
            return false;
        }
        if (((modem & MS_DSR_ON) != 0) == high) {
            return true;
        }
        Sleep(1);
    }
    return false;
}

/* The handshake from the SC64 docs: DTR high, wait for DSR high, clear, DTR low, wait for DSR low. */
static bool reset_link (void) {
    EscapeCommFunction(port, SETDTR);
    if (!dsr_is(true)) {
        snprintf(error_text, sizeof(error_text), "the cart did not answer the reset handshake (DSR never went high)");
        return false;
    }
    PurgeComm(port, PURGE_RXCLEAR | PURGE_TXCLEAR);
    EscapeCommFunction(port, CLRDTR);
    if (!dsr_is(false)) {
        snprintf(error_text, sizeof(error_text), "the cart did not answer the reset handshake (DSR never went low)");
        return false;
    }
    return true;
}

bool sc64_open (const char *name) {
    if (name) {
        snprintf(port_name, sizeof(port_name), "%s", name);
    } else if (!find_port(port_name, sizeof(port_name))) {
        snprintf(error_text, sizeof(error_text), "no SummerCart64 found among the serial ports: is the USB cable in? (or give --port COMx)");
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "\\\\.\\%s", port_name);
    port = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (port == INVALID_HANDLE_VALUE) {
        fail("could not open the port (is sc64deployer or another program using it?)");
        return false;
    }
    DCB dcb = { .DCBlength = sizeof(DCB) };
    GetCommState(port, &dcb);
    dcb.BaudRate = CBR_115200;      /* the cart's FTDI chip ignores it */
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    SetCommState(port, &dcb);
    COMMTIMEOUTS timeouts = { .ReadIntervalTimeout = 50, .ReadTotalTimeoutConstant = 500, .WriteTotalTimeoutConstant = 2000 };
    SetCommTimeouts(port, &timeouts);
    if (!reset_link()) {
        sc64_close();
        return false;
    }
    return true;
}

void sc64_close (void) {
    if (port != INVALID_HANDLE_VALUE) {
        CloseHandle(port);
        port = INVALID_HANDLE_VALUE;
    }
}

static bool send_all (const void *data, DWORD length) {
    const uint8_t *p = data;
    while (length > 0) {
        DWORD done = 0;
        if (!WriteFile(port, p, length, &done, NULL) || done == 0) {
            fail("writing to the cart failed");
            return false;
        }
        p += done;
        length -= done;
    }
    return true;
}

static bool read_all (void *out, DWORD length, DWORD timeout_ms) {
    uint8_t *p = out;
    DWORD started = GetTickCount();
    while (length > 0) {
        DWORD done = 0;
        if (!ReadFile(port, p, length, &done, NULL)) {
            fail("reading from the cart failed");
            return false;
        }
        p += done;
        length -= done;
        if (length > 0 && done == 0 && GetTickCount() - started > timeout_ms) {
            snprintf(error_text, sizeof(error_text), "the cart did not answer in time");
            return false;
        }
    }
    return true;
}

static void put32 (uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static uint32_t get32 (const uint8_t *p) {
    return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | p[3];
}

static bool send_command (char id, uint32_t arg0, uint32_t arg1, const void *data, uint32_t length) {
    uint8_t head[12] = { 'C', 'M', 'D', (uint8_t) id };
    put32(head + 4, arg0);
    put32(head + 8, arg1);
    return send_all(head, sizeof(head)) && (length == 0 || send_all(data, length));
}

/* The answer to command `id`: its data into `out` (up to `room` bytes). Packets
   the cart sends on its own, and answers to other commands, are skipped. */
static bool answer (char id, void *out, uint32_t room, uint32_t *length, DWORD timeout_ms) {
    for (;;) {
        uint8_t head[8];
        if (!read_all(head, sizeof(head), timeout_ms)) {
            return false;
        }
        uint32_t size = get32(head + 4);
        bool ours = (head[3] == (uint8_t) id) && (memcmp(head, "CMP", 3) == 0 || memcmp(head, "ERR", 3) == 0);
        if (ours && size <= room) {
            if (size && !read_all(out, size, timeout_ms)) {
                return false;
            }
            if (length) {
                *length = size;
            }
            if (memcmp(head, "ERR", 3) == 0) {
                snprintf(error_text, sizeof(error_text), "the cart answered ERR to '%c'", id);
                return false;
            }
            return true;
        }
        /* not ours (or too big): read it past */
        uint8_t skip[512];
        while (size > 0) {
            uint32_t piece = (size > sizeof(skip)) ? sizeof(skip) : size;
            if (!read_all(skip, piece, timeout_ms)) {
                return false;
            }
            size -= piece;
        }
    }
}

bool sc64_identify (char out[5]) {
    uint32_t length = 0;
    memset(out, 0, 5);
    return send_command('v', 0, 0, NULL, 0) && answer('v', out, 4, &length, 2000);
}

bool sc64_read (uint32_t address, void *out, uint32_t length) {
    uint32_t got = 0;
    if (!send_command('m', address, length, NULL, 0) || !answer('m', out, length, &got, 2000)) {
        return false;
    }
    if (got != length) {
        snprintf(error_text, sizeof(error_text), "asked the cart for %lu bytes, got %lu", (unsigned long) length, (unsigned long) got);
        return false;
    }
    return true;
}

bool sc64_write (uint32_t address, const void *data, uint32_t length) {
    if (!send_command('M', address, length, data, length)) {
        return false;
    }
    uint8_t none[4];
    answer('M', none, sizeof(none), NULL, 500);   /* the docs list no answer data; take a CMP if one comes */
    return true;
}
