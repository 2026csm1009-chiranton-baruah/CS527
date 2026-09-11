#!/bin/sh
set -eu

PATCH_FILE="$(dirname "$0")/os_strncpy_warning_fix.patch"

if [ ! -f os.c ]; then
    echo "Error: os.c not found. Run this script from your assignment directory."
    exit 1
fi

patch -p0 < "$PATCH_FILE"

echo "Patch applied successfully."
echo "Now run:"
echo "  make clean"
echo "  make stress-test"
