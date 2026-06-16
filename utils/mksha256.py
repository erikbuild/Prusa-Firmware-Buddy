#!/usr/bin/env python3
"""Write the raw 32-byte SHA256 digest of a file.

Usage:
    mksha256.py <input> <output>
"""
import sys
from hashlib import sha256
from pathlib import Path


def main():
    src, dst = sys.argv[1], sys.argv[2]
    Path(dst).write_bytes(sha256(Path(src).read_bytes()).digest())


if __name__ == '__main__':
    main()
