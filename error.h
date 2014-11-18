#ifndef DMEXEC_ERROR_H
#define DMEXEC_ERROR_H

//----------------------------------------------------------------

struct vm;
void error(const char *format, ...) __attribute__ ((format (printf, 1, 2)));

//----------------------------------------------------------------

#endif
