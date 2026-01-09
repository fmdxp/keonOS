import struct
import os

MAGIC = 0x4B454F4E
VERSION = 1
FILES_DIR = "initrd" 
OUTPUT = "iso/boot/initrd.img"

def pack():
    files = [f for f in os.listdir(FILES_DIR) if os.path.isfile(os.path.join(FILES_DIR, f))]
    file_count = len(files)
    
    HEADER_STRUCT = "<I64sIIII"  # magic, name, size, offset, next_header, reserved
    header_size = struct.calcsize(HEADER_STRUCT)
    
    superblock_size = 8*4
    data_offset = superblock_size + file_count * header_size  # superblock + headers

    payloads = []
    headers = []

    for i, name in enumerate(files):
        path = os.path.join(FILES_DIR, name)
        with open(path, "rb") as f:
            data = f.read()
        size = len(data)
        
        padding_before = (4 - (data_offset % 4)) % 4
        data_offset += padding_before

        this_file_data_offset = data_offset

        next_h = superblock_size + (i + 1) * header_size if i < file_count - 1 else 0
        name_bytes = name.encode('ascii')[:63].ljust(64, b'\x00')

        print(f"[PACKER] file={name}, size={size}, start_at={this_file_data_offset}")

        header = struct.pack(
            HEADER_STRUCT,
            MAGIC,
            name_bytes,
            size,
            this_file_data_offset,
            next_h,
            0
        )
        headers.append(header)
        payloads.append(data)

        data_offset += size
    
    total_size = data_offset

    superblock = struct.pack("<IIIIIIII", MAGIC, file_count, VERSION, total_size, 0, 0, 0, 0)

    with open(OUTPUT, "wb") as f:
        f.write(superblock)
        for h in headers: f.write(h)
        for p in payloads: f.write(p)

    print(f"Packed {file_count} files into {OUTPUT}. Total size: {os.path.getsize(OUTPUT)} bytes")


if __name__ == "__main__":
    pack()