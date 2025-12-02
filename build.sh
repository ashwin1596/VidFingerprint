#!/bin/bash

set -e  # Exit on error

echo "================================"
echo "Video Fingerprinting System"
echo "Build & Run Script"
echo "================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${YELLOW}➜${NC} $1"
}

# Check if CMake is installed
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Please install CMake 3.14 or higher."
    exit 1
fi

# Check if SQLite3 is installed
if ! command -v sqlite3 &> /dev/null; then
    print_error "SQLite3 not found. Please install SQLite3 development libraries."
    exit 1
fi

print_success "Dependencies found"

# Create build directory
print_info "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
print_info "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
print_info "Building project..."
make -j$(nproc)

print_success "Build complete!"
echo ""

# Ask user what to run
echo "What would you like to run?"
echo "1) Demo application"
echo "2) Run tests"
echo "3) Run performance benchmark"
echo "4) Run concurrency benchmark"
echo "5) All of the above"
echo ""
read -p "Enter choice [1-5]: " choice

case $choice in
    1)
        print_info "Running demo..."
        ./vfs_demo
        ;;
    2)
        print_info "Running tests..."
        ctest -V
        ;;
    3)
        print_info "Running performance benchmark..."
        ./benchmark_performance
        ;;
    4)
        print_info "Running concurrency benchmark..."
        ./benchmark_concurrency
        ;;
    5)
        print_info "Running all..."
        echo ""
        print_info "1. Running demo..."
        ./vfs_demo
        echo ""
        print_info "2. Running tests..."
        ctest -V
        echo ""
        print_info "3. Running performance benchmark..."
        ./benchmark_performance
        echo ""
        print_info "4. Running concurrency benchmark..."
        ./benchmark_concurrency
        ;;
    *)
        print_error "Invalid choice"
        exit 1
        ;;
esac

echo ""
print_success "All done!"
