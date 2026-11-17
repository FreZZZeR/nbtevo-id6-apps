/* nbtevo-id6-apps — HMI presentation (Apache-2.0).
 *
 * Pure formatting: a received frame (or a single DTC) -> a display string. No framework, no
 * car — host-testable. The plugin calls these to fill the iDrive list/label models; keeping
 * it pure means the on-car plugin only has to push the strings, and the text is tested here.
 */
#ifndef ND_HMI_VIEW_H
#define ND_HMI_VIEW_H

#include "frame.h"
#include "codes.h"   /* nd_dtc */

/* "Coolant: 50 C" / "Boost: 1.30 bar" / "RPM: 3000" when ok; "Coolant: ---" when not
 * (a not-ok gauge is never shown as a wrong number). Returns snprintf length, or -1 on a
 * bad slot. */
int nd_fmt_gauge(const nd_frame *f, int slot, char *out, int n);

/* One DTC line for the Codes list: "101A05  [08] active" / "480011  [04]". */
int nd_fmt_dtc(const nd_dtc *d, char *out, int n);

/* Codes summary tile: "No faults" / "1 fault - active" / "3 faults". */
int nd_fmt_codes(const nd_frame *f, char *out, int n);

/* xDrive status tile: "xDrive: AWD" / "xDrive: RWD (front axle off)" / "xDrive: n/a",
 * with " (variant unverified)" appended when the DSC variant isn't in the supported set. */
int nd_fmt_xdrive(const nd_frame *f, char *out, int n);

#endif /* ND_HMI_VIEW_H */
