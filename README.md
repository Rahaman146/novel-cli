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

## Usage

```bash
novel
```

## Uninstall

```bash
sudo make uninstall
```
