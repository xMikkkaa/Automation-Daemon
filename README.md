# Automation Daemon (autd)

A lightweight daemon for Android devices designed to automatically manage system performance based on the current context (active application, battery status, screen state).

This daemon is a core component of the **Aozora Kernel Helper** module (for Magisk/KSU/KSUFork) and is designed to work in tandem with the [**xBoosterCore**](https://github.com/xMikkkaa/xBoosterCore.git) application, which provides a user interface for configuration and control.

## Features

- **Dynamic Performance Switching:** Automatically switches between performance profiles.
- **Game Detection:** Identifies when a game is running and applies a specific gaming mode for optimal performance.
- **Battery Saver Integration:** Switches to a power-saving mode when the battery is low.
- **App Integration:** Can be configured and controlled via a companion app.

## Cross-Compiling for Android (aarch64)

This guide explains how to compile the `autd` binary for Android `aarch64` from a Linux, Windows, or macOS development environment. The final binary will be for Android only.

### 1. Prerequisites

Before you begin, you need to install the following tools on your development machine:

*   **Git:** To clone the repository.
*   **Make:** The build tool used to run the `Makefile`.
    *   **Linux (Debian/Ubuntu):** `sudo apt-get install git make`
    *   **Windows:** You can get `make` by installing [Chocolatey](https://chocolatey.org/) and then running `choco install make`. Git can be installed from [git-scm.com](https://git-scm.com/download/win).
    *   **macOS:** The Command Line Tools (which include `git` and `make`) can be installed by running `xcode-select --install`.
*   **Android NDK:** The Android Native Development Kit is required for cross-compilation.

### 2. Setup the Android NDK

1.  **Download:** Download the latest Android NDK from the [official Android developer website](https://developer.android.com/ndk/downloads).
2.  **Extract:** Extract the downloaded file to a stable location on your computer (e.g., `~/Android/Sdk/ndk/` or `C:\Android\ndk`).
3.  **Set Environment Variable:** You must set the `ANDROID_NDK_HOME` environment variable to point to the directory where you extracted the NDK. This is crucial for the `Makefile` to find the compiler.

    *   **Linux/macOS (add to your `.bashrc` or `.zshrc`):**
        ```bash
        export ANDROID_NDK_HOME="/path/to/your/android-ndk"
        ```
        Replace `/path/to/your/android-ndk` with the actual path.

    *   **Windows (PowerShell):**
        ```powershell
        [System.Environment]::SetEnvironmentVariable('ANDROID_NDK_HOME', 'C:\path\to\your\android-ndk', 'User')
        ```
        Replace `C:\path\to\your\android-ndk` with the actual path. You may need to restart your terminal for the change to take effect.

### 3. Clone the Repository

Clone this repository to your local machine:

```bash
git clone https://github.com/xMikkkaa/Automation-Daemon.git
cd Automation-Daemon
```


### 4. Compile the Binary

Once your environment is set up and you are in the project's root directory, simply run `make`.

```bash
make
```

This command will use the `Makefile` and the Android NDK to compile the source code into an `aarch64` Android binary. The `Makefile` is designed to automatically detect your host OS (Linux, Windows, or macOS) and use the correct toolchain from the NDK.

The compiled binary, named `autd`, will be created in the root of the project directory.

### How to Use

The compiled `autd` binary is for Android and must be copied to `/system/bin/` on Aozora Kernel Helper Module. It requires root access to run.

```sh
# Example of running the daemon via terminal on android
su -c autd
or
nohup autd
```
