# TCP File Transfer Utility

A cross-platform C-based TCP file transfer utility that allows transferring files between networked computers.

## Prerequisites

### 1. Install GCC Compiler
Ensure you have the GCC compiler installed.
- **Windows:** Install [MinGW-w64](https://www.mingw-w64.org/) or [MSYS2](https://www.msys2.org/).
- **Linux:** `sudo apt install build-essential`
- **macOS:** `xcode-select --install`

### 2. Install Make
`make` is used to automate the build process using the provided `Makefile`.

#### Windows
- **Using Chocolatey:** `choco install make`
- **Using Winget:** `winget install GnuWin32.Make`
- **Using MSYS2:** `pacman -S make`
- **Manual:** Download from [GnuWin32](http://gnuwin32.sourceforge.net/packages/make.htm) and add the `bin` folder to your System PATH.

#### Linux (Ubuntu/Debian)
```bash
sudo apt update
sudo apt install make
```

#### macOS
```bash
brew install make
```

---

## Building the Project

Open your terminal in the project directory and run:

```bash
make all
```

This will generate two executables:
- `server.exe` (or `server` on Linux/macOS)
- `client.exe` (or `client` on Linux/macOS)

To clean the build files:
```bash
make clean
```

---

## How to Use

The utility is configured to transfer a file named `send.txt` from the client to the server.

### Step 1: Prepare the File
Ensure a file named `send.txt` exists in the same directory as the client executable.

### Step 2: Start the Server
The server must be running and listening before the client connects.
```bash
./server
```
The server will listen on `127.0.0.1:8080` by default and save the received data to `recv.txt`.

### Step 3: Run the Client
In a new terminal window, run:
```bash
./client
```
The client will connect to the server, send the contents of `send.txt`, and close the connection.

---

## Configuration
Currently, the IP address and Port are hardcoded in `server.c` and `client.c`:
- **IP:** `127.0.0.1`
- **Port:** `8080`

To transfer files between different machines, update the `ip` variable in both files with the server's LAN IP address and recompile using `make`.
