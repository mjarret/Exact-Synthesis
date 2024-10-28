Here is the updated content of the `CMakeLists.txt` file:

```
cmake_minimum_required(VERSION 3.10)
project(project-name)

# Add the necessary configurations for building and running tests using gtest
add_subdirectory(tests)

# Add your source files and target executable here
# For example:
# add_executable(my_app src/main.cpp)

```

And here is the content of the `tests/test_main.cpp` file:

```cpp
#include <gtest/gtest.h>

// Add your test cases and assertions here to verify the functionality of the code

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

Please note that you need to have the `gtest` library installed and properly configured in your workspace for this setup to work.