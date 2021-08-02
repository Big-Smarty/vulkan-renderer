#include <gtest/gtest.h>

#include "inexor/vulkan-renderer/meta.hpp"

#include <iostream>

int main(int argc, char **argv) {
    using namespace inexor::vulkan_renderer;

    // Print engine and application metadata.
    std::cout << ENGINE_NAME << ", version " << ENGINE_VERSION_STR << std::endl;
    std::cout << "Configuration: " << ENGINE_BUILD_TYPE << ", Git SHA " << ENGINE_GIT << std::endl;

    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();

    std::cout << "Press Enter to close" << std::endl;
    std::cin.get();
}
