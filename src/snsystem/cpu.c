/*
 * Processor time (part of SNSYSTEM.DLL), which Windows 3.1 does not count.
 *   Idle: the kernel calls INT 2Fh AX=1689h over and over while no program
 *     has anything to do. The time inside those calls is idle, and so are
 *     the short gaps between two of them (the kernel's idle loop itself).
 *   Programs: TOOLHELP tells of every task switch; the task that ran until
 *     then is charged the time since the last switch, less the idle in it.
 * The clock is the timer device's (VTD), in 1193182 Hz ticks. DOS sessions
 * run in their own virtual machines, inside those idle calls: their time
 * counts as idle.
 */
#include "snsystem.h"
#undef MAX_PATH                         /* toolhelp.h has its own */
#include <toolhelp.h>
#include <i86.h>

#define TICKS_MS    1193                /* clock ticks per millisecond */
#define GAP_IDLE    (TICKS_MS / 2)      /* shorter gaps between idle calls are idle too */
#define MAX_TASKS   48

typedef struct {
    DWORD ms;
    WORD  rem;                          /* ticks not yet a whole ms */
} TIME;

static DWORD vtd;                       /* the timer device's entry point */
static DWORD old2F;                     /* the INT 2Fh handler before ours */
static DWORD idleTicks;                 /* runs over every hour; callers take differences */
static DWORD enterAt, leftAt;           /* the last idle call */
static BOOL afterSwitch;                /* the gap since the last idle call holds a switch */
static DWORD since, idleSince;          /* the last charge */
static HTASK running;
static TIME idleTime;
static struct { HTASK task; TIME used; } tasks[MAX_TASKS];
static BOOL started;

static DWORD Clock(void)
{
    DWORD t = 0;                        /* set by the assembly */
    _asm {
        mov ax, 0100h
        call dword ptr vtd
        mov dword ptr t, eax
    }
    return t;
}

static void Add(TIME *t, DWORD ticks)
{
    t->ms += ticks / TICKS_MS;
    t->rem += (WORD)(ticks % TICKS_MS);
    if (t->rem >= TICKS_MS) {
        t->rem -= TICKS_MS;
        t->ms++;
    }
}

/* The clock can step back a little (the timer chip is read on the fly):
   such a difference is no time at all, not an hour. */
static void Idle(DWORD ticks)
{
    if ((long)ticks <= 0)
        return;
    idleTicks += ticks;
    Add(&idleTime, ticks);
}

/* Near, in this segment: called from Hook2F with only DS set. */
static void __near IdleEnter(void)
{
    enterAt = Clock();
    if (!afterSwitch && enterAt - leftAt < GAP_IDLE)
        Idle(enterAt - leftAt);
    afterSwitch = FALSE;
}

static void __near IdleLeave(void)
{
    leftAt = Clock();
    Idle(leftAt - enterAt);
}

/* INT 2Fh: 1689h is timed around the previous handler; everything else goes
   straight on to it (its address is put on the stack and returned to). */
static void __declspec(naked) __far Hook2F(void)
{
    _asm {
        cmp ax, 1689h
        je idle
        push ax
        push ax
        push bp
        mov bp, sp
        push ds
        push ax
        mov ax, SEG old2F
        mov ds, ax
        mov ax, word ptr old2F
        mov [bp+2], ax
        mov ax, word ptr old2F+2
        mov [bp+4], ax
        pop ax
        pop ds
        pop bp
        retf
    idle:
        push ds
        push es
        pushad
        mov ax, SEG old2F
        mov ds, ax
        call IdleEnter
        popad
        pushad
        pushf
        call dword ptr old2F
        popad
        pushad
        call IdleLeave
        popad
        pop es
        pop ds
        iret
    }
}

static int Slot(HTASK t)
{
    int i, free = -1;

    for (i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task == t)
            return i;
        if (!tasks[i].task && free < 0)
            free = i;
    }
    if (free >= 0) {
        tasks[free].task = t;
        tasks[free].used.ms = tasks[free].used.rem = 0;
    }
    return free;
}

/* The running task is charged up to now. */
static void Charge(void)
{
    DWORD now = Clock();
    long ticks = (long)(now - since - (idleTicks - idleSince));
    int i;

    if (running && ticks > 0 && (i = Slot(running)) >= 0)
        Add(&tasks[i].used, ticks);
    since = now;
    idleSince = idleTicks;
}

/* From TOOLHELP, inside the scheduler: no yielding, nothing that could. */
BOOL CALLBACK __export CpuNotifyProc(WORD id, DWORD data)
{
    HTASK t = GetCurrentTask();
    int i;

    if (id == NFY_TASKIN) {
        Charge();
        running = t;
        afterSwitch = TRUE;
    } else if (id == NFY_EXITTASK) {    /* its handle may come back for another */
        Charge();
        for (i = 0; i < MAX_TASKS; i++)
            if (tasks[i].task == t)
                tasks[i].task = NULL;
        if (running == t)
            running = NULL;
    }
    return FALSE;
}

BOOL SNSYSTEM_API CpuStart(void)
{
    WORD sel = FP_SEG(Hook2F), ofs = FP_OFF(Hook2F);

    if (started)
        return TRUE;
    _asm {
        mov ax, 1684h                   /* the VTD's protected-mode entry */
        mov bx, 0005h
        xor di, di
        mov es, di
        int 2Fh
        mov word ptr vtd, di
        mov word ptr vtd+2, es
    }
    if (!vtd)
        return FALSE;
    running = GetCurrentTask();
    since = enterAt = leftAt = Clock();
    if (!NotifyRegister(NULL, (LPFNNOTIFYCALLBACK)CpuNotifyProc, NF_TASKSWITCH))
        return FALSE;
    _asm {
        mov ax, 0204h                   /* DPMI: get, then set the INT 2Fh vector */
        mov bl, 2Fh
        int 31h
        mov word ptr old2F, dx
        mov word ptr old2F+2, cx
        mov ax, 0205h
        mov bl, 2Fh
        mov cx, sel
        mov dx, ofs
        int 31h
    }
    started = TRUE;
    return TRUE;
}

/* Restores INT 2Fh only if no one has hooked it since (they would be cut
   off). Otherwise Hook2F stays in their chain: the library must stay loaded
   (keep; not from WEP, where it is already going). */
void CpuEnd(BOOL keep)
{
    char me[MAX_PATH];
    WORD sel = 0, ofs = 0;              /* set by the assembly */

    if (!started)
        return;
    NotifyUnRegister(NULL);
    _asm {
        mov ax, 0204h
        mov bl, 2Fh
        int 31h
        mov sel, cx
        mov ofs, dx
    }
    if (sel == FP_SEG(Hook2F) && ofs == FP_OFF(Hook2F)) {
        _asm {
            mov ax, 0205h
            mov bl, 2Fh
            mov cx, word ptr old2F+2
            mov dx, word ptr old2F
            int 31h
        }
    } else if (keep) {
        GetModuleFileName(g_hInst, me, sizeof me);
        LoadLibrary(me);
    }
    started = FALSE;
}

void SNSYSTEM_API CpuStop(void)
{
    CpuEnd(TRUE);
}

DWORD SNSYSTEM_API CpuClock(void)
{
    return started ? Clock() : 0;
}

DWORD SNSYSTEM_API CpuIdle(void)
{
    return idleTicks;
}

DWORD SNSYSTEM_API CpuTime(HTASK task)
{
    int i;

    if (!started)
        return 0;
    Charge();
    if (!task)
        return idleTime.ms;
    for (i = 0; i < MAX_TASKS; i++)
        if (tasks[i].task == task)
            return tasks[i].used.ms;
    return 0;
}
