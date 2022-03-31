#ifndef OSBIND_H_
#define OSBIND_H_

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#else /* _WIN32 */
#include <unistd.h>
#endif /* _WIN32 */
#include <stdlib.h>

#define FO_RDONLY O_RDONLY
#define FO_WRONLY O_WRONLY
#define FO_CREATE (O_CREAT | O_TRUNC)

#ifdef _WIN32
#define FO_BINARY _O_BINARY

#define Fopen(a, b) _open((a), (b), _S_IREAD | _S_IWRITE)
#define Fclose(a) _close(a)
#define Fread(f, s, p) _read((f), (p), (s))
#define Fwrite(f, s, p) _write((f), (p), (s))
#define Fseek(o, f, w) _lseek((f), (o), (w))
#else /* _WIN32 */
#define FO_BINARY 0

#define Fopen(a, b) open((a), (b), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define Fclose(a) close(a)
#define Fread(f, s, p) read((f), (p), (s))
#define Fwrite(f, s, p) write((f), (p), (s))
#define Fseek(o, f, w) lseek((f), (o), (w))
#endif /* _WIN32 */

#define Malloc(s) malloc(s)
#define Mfree(p) free(p)

#endif /* OSBIND_H_ */
