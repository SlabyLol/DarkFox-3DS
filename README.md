# DarkFox-3DS

A multi-purpose utility application for the Nintendo 3DS Homebrew Launcher, providing essential system information and basic tools.

## Features

*   **System Information**: View your 3DS model, firmware version, battery status, and SD card free space.
*   **Basic File Browser**: (Planned for future updates) Browse files on your SD card.
*   **Utility Tools**: (Planned for future updates) Control screen brightness, toggle WiFi, and view detailed battery health.

## Installation Guide

To use DarkFox-3DS, you will need a Nintendo 3DS console with a working Homebrew Launcher setup. You will also need the `devkitPro` toolchain installed on your computer to compile the application.

### Prerequisites

1.  **Nintendo 3DS with Homebrew Launcher**: Ensure your 3DS is capable of running homebrew applications. Refer to [3ds.hacks.guide](https://3ds.hacks.guide/) for instructions on setting up the Homebrew Launcher.
2.  **devkitPro**: Install the `devkitPro` toolchain on your computer. This includes `devkitARM` and `libctru`, which are essential for 3DS homebrew development. You can find installation instructions on the [devkitPro website](https://devkitpro.org/wiki/Getting_Started).
    *   **Windows**: Use the graphical installer from [devkitPro GitHub releases](https://github.com/devkitPro/installer/releases/latest).
    *   **Unix-like (Linux/macOS)**: Follow the `pacman` instructions on the [devkitPro pacman GitHub](https://github.com/devkitPro/pacman/releases/latest) and then run `sudo (dkp-)pacman -S 3ds-dev`.

### Building DarkFox-3DS

1.  **Clone the Repository (or download the source code)**:
    ```bash
    git clone https://github.com/YOUR_USERNAME/DarkFox-3DS.git
    cd DarkFox-3DS
    ```
    *(Note: Replace `YOUR_USERNAME` with the actual repository owner's username once hosted.)*

2.  **Compile the Application**:
    Navigate to the `DarkFox-3DS` directory in your terminal or MSYS environment (for Windows users) and run `make`:
    ```bash
    make
    ```
    This will compile the source code and generate `DarkFox-3DS.3dsx` and `DarkFox-3DS.smdh` files in the project root directory.

### Running on your 3DS

1.  **Copy Files to SD Card**:
    Copy the generated `DarkFox-3DS.3dsx` and `DarkFox-3DS.smdh` files to the `/3ds/` folder on your 3DS's SD card.

2.  **Launch from Homebrew Launcher**:
    Insert the SD card back into your 3DS, power it on, and launch the Homebrew Launcher. You should see "DarkFox-3DS" listed. Select it to start the application.

## Usage

*   **Navigation**: Use the D-Pad (Up/Down) to navigate through menu options.
*   **Select**: Press the `A` button to select a menu item.
*   **Back**: Press the `B` button to go back to the previous menu or screen.
*   **Exit**: Press the `START` button to exit the application and return to the Homebrew Launcher.

## Current Status

This is an initial release with the System Information viewer and About screen implemented. The File Browser and additional Utility Tools are planned for future updates.

## Contributing

Contributions are welcome! If you have ideas for new features, improvements, or bug fixes, please feel free to open an issue or submit a pull request.

## License

This project is open-source and distributed under the MIT License. See the `LICENSE` file for more details. (A `LICENSE` file will be added in a future update.)

## Credits

*   Developed by the DarkFox Team.
*   Built with `devkitPro` and `libctru`.
*   Special thanks to the 3DS homebrew community for their resources and tools.
