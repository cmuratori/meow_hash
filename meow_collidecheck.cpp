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

static void hash_dir(dirptr &dir, string root, unordered_set<meow_lane> &collisions, unordered_map<meow_lane, list<pair<string, string>>> &hashes, size_t &checked_files, uint64_t &total_size)
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

            hash_dir(nextdir, NEXT_PATH, collisions, hashes, checked_files, total_size);
        } else if (ent->d_type == DT_REG) {
            // regular file entity
            fdptr ffd = openat(dfd, &ent->d_name[0], O_RDONLY | O_BINARY | O_NOFOLLOW);
            if (ffd.fd == -1) {
                perror_str("could not open file for reading: " + NEXT_PATH);
                continue;
            }

#if defined(WIN32) || defined(_WIN32)
            // TODO(qix-): sorry I don't have windows D:
            #error "mmapping isn't supported on windows yet"
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

            meow_lane result = MeowHash1(0, baseptr.len, baseptr.ptr);
            string result_sha512 = sha512(string((char *) baseptr.ptr, baseptr.len));
            auto &hashlist = hashes[result];
            if (hashlist.size() > 0) {
                // skip if its sha512 is already in there as we don't need to report duplicate
                // files as collisions.
                for (const auto &filemeta : hashlist) {
                    if (filemeta.second == result_sha512) {
                        goto skip_file;
                    }
                }

                collisions.insert(result);
                goto insert_file;
            } else {
insert_file:
                hashlist.push_back(make_pair(NEXT_PATH, result_sha512));
            }

skip_file:
            total_size += filestat.st_size;
#endif

            ++checked_files;
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
    unordered_map<meow_lane, list<pair<string, string>>> hashes; // <meow_lane, [<filename, sha512_hash>]>

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

    hash_dir(root_dir, cwd, collisions, hashes, checked_files, total_size);

    cerr << "num. files hashed:  " << checked_files << endl;
    cerr << "total bytes hashed: " << total_size << " (" << human_size(total_size) << ")" << endl;
    cerr << "collisions:         " << collisions.size() << endl;

    for (const auto &collision : collisions) {
        cout << collision << endl;
        for (const auto &filemeta : hashes[collision]) {
            cout << "\t" << filemeta.first << endl;
            cout << "\t\t" << filemeta.second << endl;
        }
    }

    return collisions.size() > 0;
}
