enum ieee_fp {
    IEEE_GUESS=0,
    IEEE_LE,
    IEEE_BE,
    IEEE_NOT=-1,
};

extern enum ieee_fp detect_ieee_fp(void);

