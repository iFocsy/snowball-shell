/* The shell's message loop, and Windows kept running during long work. */
#include "internal.h"

/* For long work in the shell's own task: handles waiting messages (those
   of dlg as dialog messages) and gives other programs a turn. A message
   to itself keeps this task runnable, so Windows comes back as soon as
   they yield, not at the next timer. FALSE: Windows is ending. */
BOOL MessageLoopPump(HWND dlg)
{
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD)) {
        if (msg.message == WM_QUIT) {           /* put back for the main loop */
            PostQuitMessage(msg.wParam);
            return FALSE;
        }
        if (!dlg || !IsDialogMessage(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    PostAppMessage(GetCurrentTask(), WM_NULL, 0, 0L);
    Yield();
    return TRUE;
}

static HWND activeDialog;

void MessageLoopTrackDialog(HWND dlg, BOOL active)
{
    if (active)
        activeDialog = dlg;
    else if (activeDialog == dlg)
        activeDialog = NULL;
}

BOOL MessageLoopDialogMessage(MSG FAR *msg)
{
    return activeDialog && IsDialogMessage(activeDialog, msg);
}

/* Until Windows ends; the active modeless dialog gets its keys. */
int MessageLoopRun(void)
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0)) {
        if (MessageLoopDialogMessage(&msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
