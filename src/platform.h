/* platform.h – tiny shim for Win32 / POSIX  ---------------------------- */
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #include <direct.h>    /* _mkdir  */
    #include <io.h>        /* _filelength */
    #define MKDIR(p)          _mkdir(p)
    static inline long fd_file_size(int fd) { return _filelength(fd); }
#else
    #include <sys/stat.h>  /* mkdir, fstat */
    #include <unistd.h>
    #define MKDIR(p)          mkdir((p), 0755)
    static inline long fd_file_size(int fd)
    {
        struct stat st;
        return (fstat(fd, &st) == 0) ? (long)st.st_size : 0L;
    }
#endif

#endif /* PLATFORM_H */
