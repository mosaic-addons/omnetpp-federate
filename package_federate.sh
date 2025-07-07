#!/bin/bash

print_help() {
    echo "Usage: ./package_federate.sh [DEST_DIR]"
    echo "  DEST_DIR   Optional. Destination directory for the zip file."
    echo "             Defaults to \$HOME/eclipse-mosaic-25.0/bin/fed/omnetpp"
    echo "  -h, --help Show this help message and exit."
}

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    print_help
    exit 0
fi

SRC_DIR="$(pwd)"
# Allow DEST_DIR to be specified as the first argument, or use default
DEST_DIR="${1:-$HOME/eclipse-mosaic-25.0/bin/fed/omnetpp}"
FOLDER_NAME="omnetpp-federate-25.0"
ZIP_NAME="25.0.zip"

# Remove any previous folder to avoid recursion
cd "$SRC_DIR" || exit
rm -rf "$FOLDER_NAME"

# Copy all files except the destination folder itself
rsync -a --exclude ".git" --exclude ".gitignore" --exclude ".vscode" ./ "$FOLDER_NAME"

# Zip the folder
zip -r "$ZIP_NAME" "$FOLDER_NAME"

# Create destination directory if it doesn't exist
if ! mkdir -p "$DEST_DIR"; then
    echo "Error: Could not create destination directory: $DEST_DIR"
    exit 1
fi

# Move the zip file to the destination
mv "$ZIP_NAME" "$DEST_DIR"
rm -rf "$FOLDER_NAME"

echo "Done: $ZIP_NAME moved to $DEST_DIR"
echo "To specify a different destination, run: ./package_federate.sh /your/destination/path"

# cd "$DEST_DIR" || exit

# ./install.sh -q