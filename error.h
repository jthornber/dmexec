#ifndef DMEXEC_ERROR_H
#define DMEXEC_ERROR_H

//----------------------------------------------------------------

struct vm;
void error(struct vm *vm, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

//----------------------------------------------------------------

#endif
