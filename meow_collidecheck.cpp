#ifdef WIN32
#include <windows.h>
#define meow_getcwd _getcwd
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define meow_getcwd getcwd
#define O_BINARY 0 /* because unix doesn't make the distinction :D */
#endif

#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "meow_hash.h"
#include "./sha512.h"

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

using namespace std;

struct dirptr
{
    dirptr(DIR * const &d) : d(d)
    {
    }

    ~dirptr()
    {
        if (d != NULL) {
            closedir(d);
        }
    }

    DIR *d;
};

struct fdptr
{
    fdptr(const int &fd) : fd(fd)
    {
    }

    ~fdptr() {
        if (fd != -1) {
            close(fd);
        }
    }

    int fd;
};

struct map_region
{
    map_region(void *ptr, size_t len) : ptr(ptr), len(len)
    {
    }

    ~map_region()
    {
        if (ptr != NULL) {
            munmap(ptr, len);
        }
    }

    void *ptr;
    size_t len;
};

static void perror_str(string s)
{
    perror(s.c_str());
}


#if defined(WIN32) || defined(_WIN32)
static void perror_win32(string s) {
    // https://stackoverflow.com/a/17387176
    // modified a bit to simply output the message
    string message;

    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        goto output_message;
    }

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    message = string(messageBuffer, size);
    LocalFree(messageBuffer);

output_message:
    cerr << s << ": " << message;
}
#endif

static const char * human_size(uint64_t bytes)
{
    // credit: @dgoguerra (GH)
    // https://gist.github.com/dgoguerra/7194777
    // modified by @qix- (GH) for a few more types and include the 'i' (since 1024 and not 1000)
    const char *suffix[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    char length = sizeof(suffix) / sizeof(suffix[0]);

    int i = 0;
    double dblBytes = bytes;

    if (bytes > 1024) {
        for (i = 0; (bytes / 1024) > 0 && i<length-1; i++, bytes /= 1024)
            dblBytes = bytes / 1024.0;
    }

    static char output[200];
    sprintf(output, "%.02lf %s", dblBytes, suffix[i]);
    return output;
}

static void hash_dir(dirptr &dir, string root, unordered_set<meow_lane> &collisions, unordered_map<meow_lane, list<string>> &hashes, unordered_set<string> &sha512_hashes, size_t &checked_files, uint64_t &total_size)
{
    // NOTE(qix-): allows us to lazily build up the string exactly once
    //             and skip cases where the entity type isn't a regular
    //             file or a directory for just a small speed increase.
#   define NEXT_PATH (root + PATH_SEP + &ent->d_name[0])

    // directory entity
    int dfd = dirfd(dir.d);
    if (dfd == -1) {
        perror_str("could not get directory file descriptor: " + root);
        return;
    }

    dirent *ent = NULL;
    while ((ent = readdir(dir.d)) != NULL) {
        if (ent->d_type == DT_DIR) {
            if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
                // skip '.' and '..' which would cause an endless loop
                continue;
            }

            // NOTE(qix-): we don't wrap with an fdptr here because
            //             it is closed automatically by the call to
            //             closedir(), assuming fdopendir() works.
            int nextfd = openat(dfd, &ent->d_name[0], O_RDONLY | O_NOFOLLOW);
            if (nextfd == -1) {
                perror_str("could not open directory: " + NEXT_PATH);
                continue;
            }

            dirptr nextdir = fdopendir(nextfd);
            if (nextdir.d == NULL) {
                perror_str("could not open directory handle: " + NEXT_PATH);
                close(nextfd);
                continue;
            }

            hash_dir(nextdir, NEXT_PATH, collisions, hashes, sha512_hashes, checked_files, total_size);
        } else if (ent->d_type == DT_REG) {
            // regular file entity
            fdptr ffd = openat(dfd, &ent->d_name[0], O_RDONLY | O_BINARY | O_NOFOLLOW);
            if (ffd.fd == -1) {
                perror_str("could not open file for reading: " + NEXT_PATH);
                continue;
            }

            void *data_to_hash = nullptr;
            size_t length_to_hash;

#if defined(WIN32) || defined(_WIN32)
            HANDLE open_win32_handle = (HANDLE) _get_osfhandle(ffd.fd);
            if (open_win32_handle == INVALID_HANDLE_VALUE) {
                perror_str("could not convert stdc fd to windows handle: " + NEXT_PATH);
                continue;
            }

            // NOTE(qix-): we opt to manually manage handles here since the Win32 API is a bit
            //             awkward when it comes to file views
            ffd.fd = -1; // don't do anything upon destruction

            HANDLE file_mapping = CreateFileMappingA(open_win32_handle, PAGE_READONLY, 0, 0);
            if (file_mapping == NULL) {
                perror_win32("could not create file mapping: " + NEXT_PATH);
                CloseHandle(open_win32_handle);
                continue;
            }

            DWORD data_size = GetFileSize(open_win32_handle, NULL); // TODO(qix-): Should we support high-order dword values for sizes?
            if (data_ptr == INVALID_FILE_SIZE) {
                perror_win32("could not retrieve file size: " + NEXT_PATH);
                CloseHandle(file_mapping);
                CloseHandle(open_win32_handle);
                continue;
            }

            LPVOID data_ptr = MapViewOfFile(file_mapping, FILE_MAP_READ, 0, 0, 0);
            if (data_ptr == NULL) {
                perror_win32("could not map file view: " + NEXT_PATH);
                CloseHandle(file_mapping);
                CloseHandle(open_win32_handle);
                continue;
            }

            data_to_hash = data_ptr;
            length_to_hash = data_size;
#else
            struct stat filestat;
            if (fstat(ffd.fd, &filestat) == -1) {
                perror_str("could not stat file: " + NEXT_PATH);
                continue;
            }

            map_region baseptr(mmap(NULL, filestat.st_size, PROT_READ, MAP_NOCACHE | MAP_FILE | MAP_PRIVATE, ffd.fd, 0), filestat.st_size);
            if (baseptr.ptr == NULL) {
                perror_str("could not mmap file: " + NEXT_PATH);
                continue;
            }

            data_to_hash = baseptr.ptr;
            length_to_hash = baseptr.len;
#endif

            string result_sha512 = sha512(string((char *) data_to_hash, length_to_hash));
            const auto sha512_insertion = sha512_hashes.insert(result_sha512);
            if (!sha512_insertion.second) {
                // we're looking at duplicate information; don't consider it.
                goto skip_file;
            }

            {
                meow_lane result = MeowHash1(0, length_to_hash, data_to_hash);
                auto &hashlist = hashes[result];
                if (hashlist.size() > 0) {
                    collisions.insert(result);
                }

                hashlist.push_back(NEXT_PATH);
            }

skip_file:
            ++checked_files;
            total_size += length_to_hash;

#if defined(WIN32) || defined(_WIN32)
            UnmapViewOfFile(data_ptr);
            CloseHandle(file_mapping);
            CloseHandle(open_win32_handle);
#else
            // no cleanup necessary; handled with destructors.
#endif
        }

        // NOTE(qix-): we purposefully ignore anything that isn't a
        //             directory or regular file
    }

#undef NEXT_PATH
}

int main(int argc, const char **argv)
{
    // NOTE(qix-): this isn't the proper way to handle getcwd()
    //             but this is a test tool and cross-platform
    //             getcwd() isn't as easy as it sounds.
    static char cwdbuf[4096];
    meow_getcwd(cwdbuf, sizeof(cwdbuf));
    char *cwd = cwdbuf;

    if (argc == 2) {
        if (string(argv[1]) == "--help") {
            goto usage;
        }

        strncpy(cwd, argv[1], sizeof(cwdbuf));
    } else if (argc > 2) {
        cerr << "error: too many arguments (expected 0 or 1) " << endl;
usage:
        cerr << "usage: " << argv[0] << " [path]" << endl;
        return 2;
    }

    size_t checked_files = 0;
    uint64_t total_size = 0;
    unordered_set<meow_lane> collisions;
    unordered_set<string> sha512_hashes;
    unordered_map<meow_lane, list<string>> hashes;

    dirptr root_dir = opendir(cwd);
    if (root_dir.d == NULL) {
        perror_str("could not open root directory: " + string(cwd));
        return 1;
    }

    // hack to remove trailing slash - must come after root dir opening
    // so as to not break the case where the user passes '/'
    size_t cwdlen = strlen(cwd);
    if (cwd[cwdlen - 1] == *PATH_SEP) {
        cwd[cwdlen - 1] = 0;
    }

    hash_dir(root_dir, cwd, collisions, hashes, sha512_hashes, checked_files, total_size);

    cerr << "num. files hashed:  " << checked_files << endl;
    cerr << "num considered:     " << sha512_hashes.size() << endl;
    cerr << "num skipped:        " << (checked_files - sha512_hashes.size()) << endl;
    cerr << "total bytes hashed: " << total_size << " (" << human_size(total_size) << ")" << endl;
    cerr << "collisions:         " << collisions.size() << endl;

    for (const auto &collision : collisions) {
        cout << collision << endl;
        for (const auto &filename : hashes[collision]) {
            cout << "\t" << filename << endl;
        }
    }

    return collisions.size() > 0;
}
