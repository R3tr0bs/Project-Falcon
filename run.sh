
set -e # Stop script on error

# Ensure the build script is executable
chmod +x build.sh

# Build the project
echo "Building project..."
./build.sh

# Check if QEMU is available
if command -v qemu-system-i386 &> /dev/null; then
    echo "Running with qemu-system-i386..."
    qemu-system-i386 -kernel falcon.bin -netdev user,id=net0 -device rtl8139,netdev=net0
elif command -v qemu-system-i386.exe &> /dev/null; then
    echo "Running with qemu-system-i386.exe (Windows)..."
    qemu-system-i386.exe -kernel falcon.bin -netdev user,id=net0 -device rtl8139,netdev=net0
else
    echo "Error: qemu-system-i386 not found."
    echo "Please install QEMU or ensure it is in your PATH."
    exit 1
fi
 
