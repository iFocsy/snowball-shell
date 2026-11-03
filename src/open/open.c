/*
 * OPEN.COM: from a DOS box, opens a file, folder or program in Windows the
 * way Snowball opens them (Windows 98's START; DOSBox-X's own shell keeps
 * that name for itself):
 *   OPEN .               this folder, in a window
 *   OPEN readme.txt      the document, with its program
 *   OPEN notepad x.txt   a Windows program (with arguments)
 * The request goes to %windir%\SBOPEN.REQ: the DOS box's folder, then what
 * was typed. Snowball picks it up within a second and deletes it.
 */
#include <dos.h>
#include <stdlib.h>
#include <string.h>

static void Say(const char *s)
{
    union REGS r;

    r.h.ah = 0x40;
    r.x.bx = 1;
    r.x.cx = strlen(s);
    r.x.dx = (unsigned)s;
    intdos(&r, &r);
}

/* A variable of the environment, its name in any case. */
static int Env(const char *name, char *out, int size)
{
    char far *e = MK_FP(*(unsigned far *)MK_FP(_psp, 0x2C), 0);
    int n = strlen(name), i;

    while (*e) {
        for (i = 0; i < n && (e[i] | 0x20) == (name[i] | 0x20); i++)
            ;
        if (i == n && e[n] == '=') {
            for (e += n + 1, i = 0; *e && i < size - 1; i++)
                out[i] = *e++;
            out[i] = '\0';
            return 1;
        }
        while (*e++)
            ;
    }
    return 0;
}

/* The current folder, with long names where DOS has them. */
static void Folder(char *out)
{
    union REGS r;
    struct SREGS s;

    r.h.ah = 0x19;
    intdos(&r, &r);
    out[0] = (char)('A' + r.h.al);
    out[1] = ':';
    out[2] = '\\';
    segread(&s);
    r.x.ax = 0x7147;
    r.h.dl = 0;
    r.x.si = (unsigned)(out + 3);
    r.x.cflag = 1;
    intdosx(&r, &r, &s);
    if (r.x.cflag || r.x.ax == 0x7100) {
        r.h.ah = 0x47;
        r.h.dl = 0;
        r.x.si = (unsigned)(out + 3);
        intdosx(&r, &r, &s);
    }
}

static int Exists(const char *path)
{
    union REGS r;

    r.x.ax = 0x4300;
    r.x.dx = (unsigned)path;
    intdos(&r, &r);
    return !r.x.cflag;
}

int main(void)
{
    static char args[130], dir[300], file[160], text[440];
    unsigned char far *tail = MK_FP(_psp, 0x80);
    union REGS r;
    int n = tail[0], i, tries;

    for (i = 0; i < n && i < sizeof args - 1; i++)
        args[i] = tail[1 + i];
    args[i] = '\0';
    for (i = 0; args[i] == ' ' || args[i] == '\t'; i++)
        ;
    if (!args[i]) {
        Say("Opens a file, folder or program in Windows (Snowball).\r\n\r\n"
            "  OPEN [path] [arguments]\r\n\r\n  OPEN .          this folder\r\n"
            "  OPEN x.txt      a document, with its program\r\n");
        return 1;
    }
    r.x.ax = 0x1600;                    /* Windows in 386 enhanced mode? */
    int86(0x2F, &r, &r);
    if (r.h.al == 0 || r.h.al == 0x80) {
        Say("Windows is not running.\r\n");
        return 1;
    }
    if (!Env("windir", file, sizeof file - 12))
        strcpy(file, "C:\\WINDOWS");
    strcat(file, "\\SBOPEN.REQ");
    Folder(dir);
    strcpy(text, dir);
    strcat(text, "\r\n");
    strcat(text, args + i);
    strcat(text, "\r\n");
    for (tries = 0; Exists(file) && tries < 300; tries++) {     /* one waiting: a few seconds */
        r.x.ax = 0x1680;                /* the rest of the time slice to Windows */
        int86(0x2F, &r, &r);
    }
    r.h.ah = 0x3C;
    r.x.cx = 0;
    r.x.dx = (unsigned)file;
    intdos(&r, &r);
    if (r.x.cflag) {
        Say("Cannot leave the request for Windows.\r\n");
        return 1;
    }
    n = r.x.ax;
    r.h.ah = 0x40;
    r.x.bx = n;
    r.x.cx = strlen(text);
    r.x.dx = (unsigned)text;
    intdos(&r, &r);
    r.h.ah = 0x3E;
    r.x.bx = n;
    intdos(&r, &r);
    return 0;
}
