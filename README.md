# TurnThem

A 2D game built with Raylib where you control cannons and projectiles.

## Build Instructions

### Prerequisites
- CMake (version 3.10 or higher)
- C++ compiler with C++17 support
- Git (for fetching Raylib dependency)

### Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/TurnThem.git
   cd TurnThem
   ```

2. Create a build directory and navigate to it:
   ```bash
   mkdir build
   cd build
   ```

3. Generate build files with CMake:
   ```bash
   cmake ..
   ```

4. Build the project:
   ```bash
   cmake --build .
   ```

5. Run the game:
   ```bash
   ./TurnThem
   ```

The build system will automatically download and compile Raylib if it's not already installed on your system.