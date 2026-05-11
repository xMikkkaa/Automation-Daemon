#!/bin/bash

# Ensure ANDROID_NDK_HOME is set
if [ -z "$ANDROID_NDK_HOME" ]; then
    echo -e "\e[1;31m[!] ERROR: ANDROID_NDK_HOME is not set.\e[0m"
    echo -e "    Please export your NDK path. Example:"
    echo -e "    export ANDROID_NDK_HOME=\"/path/to/android-ndk\""
    exit 1
fi

clear
echo -e "\e[1;32m[*] Compiling Autd (Rust Edition) v2.1 for AArch64... \e[0m"

# Check and install required toolchains
if ! command -v cargo-ndk &> /dev/null; then
    echo -e "\e[1;33m[*] Installing cargo-ndk...\e[0m"
    cargo install cargo-ndk
fi

if ! rustup target list | grep -q "aarch64-linux-android (installed)"; then
    echo -e "\e[1;33m[*] Adding target aarch64-linux-android...\e[0m"
    rustup target add aarch64-linux-android
fi

# Execute build via cargo-ndk
cargo ndk -t arm64-v8a build --release

if [ $? -eq 0 ]; then
    echo -e "\n\e[1;32m[+] SUCCESS: Build completed successfully.\e[0m"
    
    # Move binary to project root
    cp target/aarch64-linux-android/release/autd ./autd
    
    echo -e "    Binary: \e[1;37m./autd\e[0m"

    # Check final binary size
    SIZE=$(du -h ./autd | cut -f1)
    echo -e "    Size:   \e[1;33m$SIZE\e[0m"

    # Generate SHA256 checksum
    sha256sum ./autd | awk '{print $1}' | tr -d '\n' > autd.sha256
    echo -e "    SHA256: \e[1;37m./autd.sha256\e[0m"
else
    echo -e "\n\e[1;31m[!] ERROR: Build failed.\e[0m"
fi