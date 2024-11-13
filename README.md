# Exact Synthesis Codebase

## Overview
This repository contains the code for exact quantum circuit synthesis on 2 qubit Clifford+T circuits. The core of the project is implemented in C++, featuring various classes, algorithms, and utility scripts for compiling circuits, analyzing results, and testing. 

## File Structure
- **Source Code**
  - `main.cpp`: The main entry point for running the synthesis algorithm.
  - `Globals.cpp/.hpp`, `SO6.cpp/.hpp`, `Z2.cpp/.hpp`, `pattern.cpp/.hpp`, `utils.hpp`: Core source and header files defining the main classes and algorithms used for synthesis.
- **Makefiles**
  - `Makefile`: Used for compiling the code. Adjust this as needed for your environment.
- **Data**
  - `data/`: Contains data files (`.dat`) used for testing and analysis.

## Prerequisites
- A C++ compiler (GCC recommended)
- `make` utility

## Installation
1. Clone the repository:
   ```sh
   git clone https://github.com/yourusername/Exact-Synthesis.git
   ```
2. Navigate into the project directory:
   ```sh
   cd Exact-Synthesis
   ```
3. Build the project using `make`:
   ```sh
   make
   ```
4. Ensure that there is a directory `data`:
   ```sh
   mkdir data
   ```

## Running the Code
After building the project, you can run it using:

```sh
./main.out
```

## Usage
- The core functionality revolves around exact synthesis algorithms using C++ classes defined in the source files.
- The `data` directory contains necessary input data that the algorithms use.

## Contributing
Please ensure any code contributions adhere to the current style and structure. Before submitting a pull request, make sure to:
- Run all relevant tests.
- Include documentation for any new features.

To contribute:
1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Commit your changes (`git commit -m 'Add new feature'`).
4. Push to the branch (`git push origin feature-branch`).
5. Open a pull request.

## License
This project is licensed under [LICENSE] (please replace with the appropriate license if available).

## Acknowledgments
- Special thanks to contributors and those who provided support for the research and development of this project.
