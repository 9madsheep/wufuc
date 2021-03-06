#include <Windows.h>
#include "patternfind.h"

/* Ported to C from x64dbg's patternfind.cpp:
 *   https://github.com/x64dbg/x64dbg/blob/development/src/dbg/patternfind.cpp
 * x64dbg license (GPL-3.0):
 *   https://github.com/x64dbg/x64dbg/blob/development/LICENSE
 */

int hexchtoint(CHAR c) {
    int result = -1;
    if (c >= '0' && c <= '9')
        result = c - '0';
    else if (c >= 'A' && c <= 'F')
        result = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        result = c - 'a' + 10;

    return result;
}

SIZE_T formathexpattern(LPCSTR patterntext, LPSTR formattext, SIZE_T formattextsize) {
    SIZE_T len = strlen(patterntext);
    SIZE_T result = 0;
    for (SIZE_T i = 0; i < len && (!formattext || result < formattextsize); i++) {
        if (patterntext[i] == '?' || hexchtoint(patterntext[i]) != -1) {
            if (formattext)
                formattext[result] = patterntext[i];

            result++;
        }
    }
    return result;
}

BOOL patterntransform(LPCSTR patterntext, LPPATTERNBYTE pattern, SIZE_T *patternsize) {
    SIZE_T cb = formathexpattern(patterntext, NULL, 0);
    if (!cb || cb > *patternsize)
        return FALSE;

    LPSTR formattext = calloc(cb, sizeof(CHAR));
    cb = formathexpattern(patterntext, formattext, cb);

    if (cb % 2)
        formattext[cb++] = '?';

    formattext[cb] = '\0';

    for (SIZE_T i = 0, j = 0, k = 0; i < cb; i++, j ^= 1, k = (i - j) >> 1) {
        if (formattext[i] == '?')
            pattern[k].nibble[j].wildcard = TRUE;
        else {
            pattern[k].nibble[j].wildcard = FALSE;
            pattern[k].nibble[j].data = hexchtoint(formattext[i]) & 0xf;
        }
    }
    free(formattext);
    *patternsize = cb >> 1;
    return TRUE;
}

LPBYTE patternfind(LPBYTE data, SIZE_T datasize, SIZE_T startindex, LPCSTR pattern) {
    LPBYTE result = NULL;
    SIZE_T searchpatternsize = strlen(pattern);
    LPPATTERNBYTE searchpattern = calloc(searchpatternsize, sizeof(PATTERNBYTE));

    if (patterntransform(pattern, searchpattern, &searchpatternsize)) {
        for (SIZE_T i = startindex, j = 0; i < datasize; i++) { //search for the pattern
            if ((searchpattern[j].nibble[0].wildcard || searchpattern[j].nibble[0].data == ((data[i] >> 4) & 0xf))
                && (searchpattern[j].nibble[1].wildcard || searchpattern[j].nibble[1].data == (data[i] & 0xf))) { //check if our pattern matches the current byte

                if (++j == searchpatternsize) { //everything matched
                    result = data + (i - searchpatternsize + 1);
                    break;
                }
            } else if (j > 0) { //fix by Computer_Angel
                i -= j;
                j = 0; //reset current pattern position
            }
        }
    }
    free(searchpattern);
    return result;
}

VOID patternwritebyte(LPBYTE byte, LPPATTERNBYTE pbyte) {
    BYTE n1 = (*byte >> 4) & 0xf;
    BYTE n2 = *byte & 0xf;
    if (!pbyte->nibble[0].wildcard)
        n1 = pbyte->nibble[0].data;

    if (!pbyte->nibble[1].wildcard)
        n2 = pbyte->nibble[1].data;
    *byte = ((n1 << 4) & 0xf0) | (n2 & 0xf);
}

BOOL patternwrite(LPBYTE data, SIZE_T datasize, LPCSTR pattern) {

    SIZE_T writepatternsize = strlen(pattern);
    if (writepatternsize > datasize) 
        writepatternsize = datasize;

    BOOL result = FALSE;
    LPPATTERNBYTE writepattern = calloc(writepatternsize, sizeof(PATTERNBYTE));
    if (patterntransform(pattern, writepattern, &writepatternsize)) {
        DWORD flNewProtect = PAGE_READWRITE;
        DWORD flOldProtect;

        if (VirtualProtect(data, writepatternsize, flNewProtect, &flOldProtect)) {
            for (size_t i = 0; i < writepatternsize; i++) {
                BYTE n1 = (data[i] >> 4) & 0xf;
                BYTE n2 = data[i] & 0xf;
                if (!writepattern[i].nibble[0].wildcard)
                    n1 = writepattern[i].nibble[0].data;

                if (!writepattern[i].nibble[1].wildcard)
                    n2 = writepattern[i].nibble[1].data;
                data[i] = ((n1 << 4) & 0xf0) | (n2 & 0xf);

            }
            result = VirtualProtect(data, writepatternsize, flOldProtect, &flNewProtect);
        }
    }
    free(writepattern);
    return result;
}

LPBYTE patternsnr(LPBYTE data, SIZE_T datasize, SIZE_T startindex, LPCSTR searchpattern, LPCSTR replacepattern) {
    LPBYTE result = patternfind(data, datasize, startindex, searchpattern);
    if (result == NULL)
        return result;

    patternwrite(result, datasize, replacepattern);
    return result;
}
