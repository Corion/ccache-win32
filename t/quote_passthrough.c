/*

Tests that we can pass through quotes
through dmake and ccache to gcc
The Perl build process uses this to define constants as strings

This file must be compiled as

gcc -DVERSION=\"0.01\"

*/
const char* version = VERSION;
