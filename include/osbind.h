#ifndef OSBIND_H_
#define OSBIND_H_

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define Fopen(a, b) open((a), (b))
#define Fclose(a) close(a)
#define Fread(f, s, p) read((f), (p), (s))
#define Fseek(o, f, w) lseek((f), (o), (w))

#define Malloc(s) malloc(s)
#define Mfree(p) free(p)

#endif /* OSBIND_H_ */
