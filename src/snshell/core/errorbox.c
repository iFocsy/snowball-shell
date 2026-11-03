/* Snowball's error messages. */
#include "internal.h"

void ErrorBox(HWND owner, LPCSTR text)
{
    MessageBox(owner, text, SB_NAME, MB_ICONEXCLAMATION | MB_OK);
}
