# SPDX-License-Identifier: GPL-3.0-or-later
"""Add Spherical Video V2 metadata to our own, non-fragmented, moov-last MP4."""

import struct


def box(tag, payload):
    return struct.pack(">I4s", len(payload) + 8, tag) + payload


def mark_spherical(path):
    def children(data, start, end):
        while start + 8 <= end:
            size, tag = struct.unpack_from(">I4s", data, start)
            if not 8 <= size <= end - start:
                raise ValueError("Invalid generated MP4 atom")
            yield start, size, tag
            start += size

    full = bytes(4)
    projection = box(b"proj", box(b"prhd", full + bytes(12)) + box(b"equi", full + bytes(16)))
    tags = box(b"st3d", full + b"\0") + box(b"sv3d", box(b"svhd", full + b"KdenCut 360\0") + projection)
    with path.open("r+b") as f:
        total = f.seek(0, 2)
        f.seek(0)
        while f.tell() + 8 <= total:
            start = f.tell()
            size, tag = struct.unpack(">I4s", f.read(8))
            if size == 1:
                (size,) = struct.unpack(">Q", f.read(8))
            if size < 8 or start + size > total:
                break
            if tag == b"moov" and start + size == total:
                f.seek(start)
                data = bytearray(f.read(size))
                break
            f.seek(start + size)
        else:
            raise ValueError("Expected final MP4 movie atom")
        if tag != b"moov" or start + size != total:
            raise ValueError("Expected final MP4 movie atom")

        def insert(offset, length, depth, parents):
            # stsd has version/flags and entry count before its sample entries.
            skip = 16 if depth == 5 else 8
            for pos, n, tag in children(data, offset + skip, offset + length):
                if depth < 5:
                    if tag == (b"trak", b"mdia", b"minf", b"stbl", b"stsd")[depth]:
                        if insert(pos, n, depth + 1, parents + [pos]):
                            return True
                elif tag in (b"avc1", b"hvc1", b"hev1"):
                    for p in parents + [pos]:
                        (old,) = struct.unpack_from(">I", data, p)
                        struct.pack_into(">I", data, p, old + len(tags))
                    data[pos + n : pos + n] = tags
                    return True
            return False

        if not insert(0, size, 0, [0]):
            raise ValueError("Could not locate the encoded video sample entry")
        f.seek(start)
        f.write(data)
