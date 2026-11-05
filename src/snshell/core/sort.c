/* Sorting of records in far memory. */
#include "internal.h"

/* Shell sort of n records of size bytes in far memory (at most 64 bytes each). */
void SortRecords(void far *base, int n, int size, int (*cmp)(const void far *, const void far *))
{
    BYTE tmp[64];
    BYTE far *b = (BYTE far *)base;
    unsigned u = size;                  /* offsets within one segment: unsigned, never negative */
    int gap, i, j;

    if (u > sizeof tmp)
        return;
    for (gap = n / 2; gap > 0; gap /= 2)
        for (i = gap; i < n; i++) {
            _fmemcpy(tmp, b + i * u, u);
            for (j = i; j >= gap && cmp(b + (j - gap) * u, tmp) > 0; j -= gap)
                _fmemcpy(b + j * u, b + (j - gap) * u, u);
            _fmemcpy(b + j * u, tmp, u);
        }
}
