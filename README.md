# Automation Daemon (autd)

A lightweight daemon for Android devices designed to automatically manage system performance based on the current context (active application, battery status, screen state).

This daemon is a core component of the [**Aozora Kernel Helper**](https://t.me/KaiProject2/1077) module (for Magisk/KSU/KSUFork) and is designed to work in tandem with the [**xBoosterCore**](https://github.com/xMikkkaa/xBoosterCore.git) app.

## Features

- **Dynamic Performance Switching:** Automatically switches between performance profiles.
- **Game Detection:** Identifies when a game is running and applies a specific gaming mode for optimal performance.
- **Battery Saver Integration:** Switches to a power-saving mode when the battery is low.
- **App Integration:** Toast notification from app.

## Cross-Compiling for Android (aarch64)

This guide explains how to compile the `autd` binary (Rust Edition) for Android `aarch64` from a Linux or macOS development environment (or WSL/Git Bash on Windows). The final binary will be for Android only.

### 1. Prerequisites

Before you begin, you need to install the following tools on your development machine:

*   **Git:** To clone the repository.
*   **Rust:** The Rust toolchain (cargo, rustc). Install via rustup.rs.
*   **Android NDK:** The Android Native Development Kit is required for cross-compilation.

### 2. Setup the Android NDK

1.  **Download:** Download the latest Android NDK from the [official Android developer website](https://developer.android.com/ndk/downloads).
2.  **Extract:** Extract the downloaded file to a stable location on your computer (e.g., `~/Android/Sdk/ndk/` or `C:\Android\ndk`).
3.  **Set Environment Variable:** You must set the `ANDROID_NDK_HOME` environment variable to point to the directory where you extracted the NDK. This is crucial for the build script to find the correct compiler.

    *   **Linux/macOS (add to your `.bashrc` or `.zshrc`):**
        ```bash
        export ANDROID_NDK_HOME="/path/to/your/android-ndk"
        ```
        Replace `/path/to/your/android-ndk` with the actual path.

    *   **Windows (WSL / Git Bash):**
        Use the Linux export command above inside your bash environment.

### 3. Clone the Repository

Clone this repository to your local machine:

```bash
git clone https://github.com/xMikkkaa/Automation-Daemon.git
cd Automation-Daemon
```


### 4. Compile the Binary

Once your environment is set up and you are in the project's root directory, simply run the compilation script.

```bash
bash compile.sh
```

The compiled binary, named `autd`, will be created in the root of the project directory.

### How to Use

The compiled `autd` binary is for Android and must be copied to `/system/bin/` on Aozora Kernel Helper Module. It requires root access to run.

```sh
# Example of running the daemon via terminal on android
su -c autd
# or
nohup autd
```
