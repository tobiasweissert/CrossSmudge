#pragma once

// A firmware image must select the Sticky SDK profile. The reusable SDK keeps
// its own hardware catalogue; Crosscub does not expose those product targets.
#if defined(ARDUINO_ARCH_ESP32) &&                                                                          \
    (!defined(FREEINK_DEVICE_STICKY) || !FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X3 || FREEINK_DEVICE_X4 || \
     FREEINK_DEVICE_X4PRO || FREEINK_DEVICE_X4CLASSIC || FREEINK_DEVICE_WS397 || FREEINK_DEVICE_ONEPAGE ||  \
     FREEINK_DEVICE_PAPERMONO || FREEINK_DEVICE_EEGO_A4 || FREEINK_DEVICE_M5 || FREEINK_DEVICE_MURPHY_M4 || \
     FREEINK_DEVICE_MURPHY || FREEINK_DEVICE_DELINK || FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_M5PAPER ||   \
     FREEINK_DEVICE_PAPERS3)
#error "Crosscub firmware requires the reTerminal Sticky profile exclusively"
#endif

// Native AWDL backend is qualified only for reTerminal Sticky. No radio code
// is retained by constrained C3 or other S3 device images.
#if defined(FREEINK_DEVICE_STICKY) && FREEINK_DEVICE_STICKY && !defined(SIMULATOR)
#define CROSSINK_APP_CAP_AIRDROP 1
#else
#define CROSSINK_APP_CAP_AIRDROP 0
#endif

// The app deliberately has a build-time touch capability, separate from a
// runtime probe. Button-only images must not retain the touch UI merely because
// the shared source tree also builds for Sticky.
#ifndef CROSSSMUDGE_APP_CAP_TOUCH
#error "Define CROSSSMUDGE_APP_CAP_TOUCH as 0 or 1 in the PlatformIO environment"
#endif

#if CROSSSMUDGE_APP_CAP_TOUCH != 0 && CROSSSMUDGE_APP_CAP_TOUCH != 1
#error "CROSSSMUDGE_APP_CAP_TOUCH must be 0 or 1"
#endif

#ifndef CROSSINK_APP_CAP_USB_DRIVE
#error "Define CROSSINK_APP_CAP_USB_DRIVE as 0 or 1 in the PlatformIO environment"
#endif

#if CROSSINK_APP_CAP_USB_DRIVE != 0 && CROSSINK_APP_CAP_USB_DRIVE != 1
#error "CROSSINK_APP_CAP_USB_DRIVE must be 0 or 1"
#endif

// Native simulator BoardConfig deliberately has no FREEINK_CAP_TOUCH macro.
// Firmware builds must keep the app and SDK capability selections in lockstep.
#if !defined(SIMULATOR)
#include <BoardConfig.h>
#if CROSSSMUDGE_APP_CAP_TOUCH != FREEINK_CAP_TOUCH
#error "CROSSSMUDGE_APP_CAP_TOUCH must match FREEINK_CAP_TOUCH"
#endif
#if CROSSINK_APP_CAP_USB_DRIVE != FREEINK_CAP_USB_MSC
#error "CROSSINK_APP_CAP_USB_DRIVE must match FREEINK_CAP_USB_MSC"
#endif
#endif
