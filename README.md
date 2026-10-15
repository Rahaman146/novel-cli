# novel-cli

A terminal-based web novel reader.

## Installation

### Quick Install (Recommended)

```bash
chmod +x install.sh
./install.sh
```

### Manual Install

**Arch Linux / Manjaro:**
```bash
yay -S ncurses curl cjson
make
sudo make install
```

**Ubuntu / Debian:**
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev libcurl4-openssl-dev libcjson-dev
make
sudo make install
```

**Fedora / RHEL:**
```bash
sudo dnf install ncurses-devel libcurl-devel cjson-devel
make
sudo make install
```

**macOS:**
```bash
brew install ncurses curl cjson
make
sudo make install
```

**Windows (MSYS2 / MinGW-w64):**

> **Tip:** For the best experience with full UTF-8 emojis and colors, run in **Windows Terminal**.

1. Open the **MSYS2 MINGW64** terminal.
2. Install the required dependencies:
```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-pdcurses mingw-w64-x86_64-curl mingw-w64-x86_64-cjson
```
3. Build using Make or CMake:
```bash
# Using Makefile
make -f Makefile.win

# Or using CMake
cmake -B build
cmake --build build
```
4. Run:
```bash
./novel-cli.exe
```

Pre-built Windows binaries are also available from GitHub Actions / Releases.

## Usage

```bash
novel
```

## Uninstall

```bash
sudo make uninstall
```
