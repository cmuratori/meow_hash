# Meow hash 0.5/calico
This is the official x64 implementation of the Meow hash, a fast hash taking 128 bytes of seed and producing 128 bits of output.  It is designed to be faster than currently available hashes in the "non-cryptographic" category.  See https://mollyrocket.com/meowhash for usage, implementation, and analysis details.

This version is v0.5 and is a **proposal for the final hash construction**.  Please provide as much testing, analysis, and commentary on the hash function as you can.  If we cannot substantially improve the hash function from here, this construction will be used for v1.0.

This version builds on Windows/MSVC, Windows/CLANG, and Linux/CLANG, with support for x64 AES-NI processors.

# Unofficial Ports
We do not maintain or vet these ports in any way.  Their correctness and performance may differ from the official Meow Hash.  **They may also have different, more restrictive licenses** than Meow Hash itself.  Use at your own risk.

* [.NET core port](https://github.com/tvandijck/meow_hash.NET) (by [Tom van Dijck](https://github.com/tvandijck))
* [Rust port](https://github.com/bodil/meowhash-rs) (by [Bodil Stokke](https://github.com/bodil))

If you have ported Meow Hash to another language, and would like to link to your repository here, please open an issue an include the information and link.
