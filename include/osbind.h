#ifndef OSBIND_H_
#define OSBIND_H_

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define FO_RDONLY O_RDONLY
#define FO_WRONLY O_WRONLY
#define FO_CREATE (O_CREAT | O_TRUNC)

#define Fopen(a, b) open((a), (b), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define Fclose(a) close(a)
#define Fread(f, s, p) read((f), (p), (s))
#define Fwrite(f, s, p) write((f), (p), (s))
#define Fseek(o, f, w) lseek((f), (o), (w))

#define Malloc(s) malloc(s)
#define Mfree(p) free(p)

#endif /* OSBIND_H_ */
