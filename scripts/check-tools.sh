#!/bin/bash

# Check for required development tools

echo "Checking development tools..."

# Colors
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

# Check for tools
check_tool() {
    local tool=$1
    local install_cmd=$2
    
    if command -v $tool &> /dev/null; then
        echo -e "${GREEN}✓${NC} $tool found"
        return 0
    else
        echo -e "${RED}✗${NC} $tool not found"
        echo -e "  ${YELLOW}Install with: $install_cmd${NC}"
        return 1
    fi
}

# Check all tools
all_found=true

check_tool "cmake" "brew install cmake" || all_found=false
check_tool "clang" "xcode-select --install" || all_found=false
check_tool "clang++" "xcode-select --install" || all_found=false
check_tool "clang-format" "brew install clang-format" || all_found=false
check_tool "clang-tidy" "brew install llvm" || all_found=false

if [ "$all_found" = true ]; then
    echo -e "\n${GREEN}All development tools are installed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tools are missing. Please install them before continuing.${NC}"
    exit 1
fi