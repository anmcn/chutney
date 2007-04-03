#include <string.h>
#include "chutneyutil.h" 
static enum ieee_fp ieee_fp_guess = IEEE_GUESS;

int detect_ieee_fp(void)
{
    double n = 19210354409446948.0;

    if (ieee_fp_guess == IEEE_GUESS) {
        if (sizeof(n) != 8)
            ieee_fp_guess = IEEE_NOT;
        else if (memcmp(&n, "\x89\x67\xa5\xcb\xed\x0f\x51\x43", 8) == 0)
            ieee_fp_guess = IEEE_LE;
        else if (memcmp(&n, "\x43\x51\x0f\xed\xcb\xa5\x67\x89", 8) == 0)
            ieee_fp_guess = IEEE_BE;
        else
            ieee_fp_guess = IEEE_NOT;
    }
    return ieee_fp_guess;
}

#ifdef TESTME
#include <stdio.h>
int main(int argc, char **argv)
{
    switch (detect_ieee_fp()) {
    case IEEE_GUESS:
        printf("guess\n"); break;
    case IEEE_LE:
        printf("IEEE LE 64 bit\n"); break;
    case IEEE_BE:
        printf("IEEE BE 64 bit\n"); break;
    case IEEE_NOT:
        printf("Not IEEE 64 bit\n"); break;
    }
}
#endif /* TESTME */
