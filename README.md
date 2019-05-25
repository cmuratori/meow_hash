# Meow hash 0.5/calico
This is the official x64 implementation of the Meow hash, a fast [Level 3](http://nohatcoder.dk/2019-05-19-1.html#level3) hash taking 128 bytes of seed and producing 128 bits of output.  It is designed to be both stronger and faster than currently available hashes in the "non-cryptographic" category.  See https://mollyrocket.com/meowhash for usage, implementation, and analysis details.

This version is v0.5 and is a **proposal for the final hash construction**.  Please provide as much testing, analysis, and commentary on the hash function as you can.  If we cannot substantially improve the hash function from here, this construction will be used for v1.0.

This version builds on Windows/MSVC, Windows/CLANG, and Linux/CLANG, with support for x64 AES-NI processors.
