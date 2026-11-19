#ifndef WIPE_H
#define WIPE_H

#include <string>
#include <vector>

// Overwrites secrets (passwords, keys) in memory once they are no longer
// needed, so they do not linger in the process. The volatile pointer keeps
// the compiler from removing the writes as "useless". Best effort: copies
// made by the standard library or the kernel are outside our control.
inline void wipe(std::string& secret)
{
    volatile char* p = secret.data();
    for (std::size_t i = 0; i < secret.size(); ++i) {
        p[i] = 0;
    }
    secret.clear();
}

inline void wipe(std::vector<unsigned char>& secret)
{
    volatile unsigned char* p = secret.data();
    for (std::size_t i = 0; i < secret.size(); ++i) {
        p[i] = 0;
    }
    secret.clear();
}

#endif // WIPE_H
