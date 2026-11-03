/* Drives as Windows 98 names them. */
#include "internal.h"

void DriveTypeName(int drive, LPSTR out)
{
    switch (GetDriveType(drive)) {
    case DRIVE_REMOVABLE: lstrcpy(out, "Removable Disk"); break;
    case DRIVE_REMOTE:    lstrcpy(out, "Network Drive"); break;
    default:              lstrcpy(out, "Local Disk");
    }
}

/* As Windows 98 names drives without a label. */
void DriveName(int drive, LPSTR out)
{
    wsprintf(out, GetDriveType(drive) == DRIVE_REMOVABLE ? "3\xbd Floppy (%c:)" : "(%c:)", 'A' + drive);
}
