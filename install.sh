#!/bin/bash

# CLI Web Novel Reader - Installation Script
# This script installs dependencies and the application system-wide

set -e  # Exit on error

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== CLI Web Novel Reader Installation ===${NC}\n"

# Detect package manager
if command -v yay &> /dev/null; then
    PKG_MANAGER="yay"
    INSTALL_CMD="yay -S --noconfirm"
elif command -v pacman &> /dev/null; then
    PKG_MANAGER="pacman"
    INSTALL_CMD="sudo pacman -S --noconfirm"
elif command -v apt-get &> /dev/null; then
    PKG_MANAGER="apt-get"
    INSTALL_CMD="sudo apt-get install -y"
elif command -v dnf &> /dev/null; then
    PKG_MANAGER="dnf"
    INSTALL_CMD="sudo dnf install -y"
elif command -v brew &> /dev/null; then
    PKG_MANAGER="brew"
    INSTALL_CMD="brew install"
else
    echo -e "${RED}Error: No supported package manager found${NC}"
    echo "Please install dependencies manually:"
    echo "  - ncurses"
    echo "  - curl"
    echo "  - cjson"
    echo "  - myhtml (optional)"
    exit 1
fi

echo -e "${YELLOW}Detected package manager: ${PKG_MANAGER}${NC}\n"

# Install dependencies based on package manager
echo -e "${GREEN}Installing dependencies...${NC}"

case $PKG_MANAGER in
    yay|pacman)
        $INSTALL_CMD ncurses curl cjson
        ;;
    apt-get)
        $INSTALL_CMD libncurses5-dev libncursesw5-dev libcurl4-openssl-dev libcjson-dev
        ;;
    dnf)
        $INSTALL_CMD ncurses-devel libcurl-devel cjson-devel
        ;;
    brew)
        $INSTALL_CMD ncurses curl cjson
        ;;
esac

echo -e "${GREEN}✓ Dependencies installed successfully${NC}\n"

# Build the application
echo -e "${GREEN}Building application...${NC}"
make clean
make

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful${NC}\n"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi

# Install system-wide
echo -e "${GREEN}Installing application system-wide...${NC}"
sudo make install

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Installation successful${NC}\n"
else
    echo -e "${RED}✗ Installation failed${NC}"
    exit 1
fi

# Verify installation
if command -v novel &> /dev/null; then
    echo -e "${GREEN}=== Installation Complete ===${NC}"
    echo -e "You can now run the application from anywhere by typing: ${YELLOW}novel${NC}"
    echo -e "\nTo uninstall, run: ${YELLOW}sudo make uninstall${NC}"
else
    echo -e "${YELLOW}Warning: Command 'novel' not found in PATH${NC}"
    echo "You may need to restart your terminal or add /usr/local/bin to your PATH"
fi
