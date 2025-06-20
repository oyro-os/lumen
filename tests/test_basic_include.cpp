#include <lumen/lumen.h>
#include <gtest/gtest.h>

TEST(BasicTest, CanIncludeHeader) {
    EXPECT_TRUE(true);
}

TEST(BasicTest, VersionMacros) {
    EXPECT_EQ(LUMEN_VERSION_MAJOR, 0);
    EXPECT_EQ(LUMEN_VERSION_MINOR, 1);
    EXPECT_EQ(LUMEN_VERSION_PATCH, 0);
    EXPECT_STREQ(LUMEN_VERSION_STRING, "0.1.0");
    
    // Test version number calculation
    EXPECT_EQ(LUMEN_VERSION_NUMBER, 100);  // 0*10000 + 1*100 + 0
    
    // Version functions should match macros
    EXPECT_EQ(lumen_version_major(), LUMEN_VERSION_MAJOR);
    EXPECT_EQ(lumen_version_minor(), LUMEN_VERSION_MINOR);
    EXPECT_EQ(lumen_version_patch(), LUMEN_VERSION_PATCH);
    EXPECT_STREQ(lumen_version_string(), LUMEN_VERSION_STRING);
}

TEST(BasicTest, PlatformDetection) {
    // At least one platform should be defined
    bool has_platform = false;
    
#ifdef LUMEN_PLATFORM_WINDOWS
    has_platform = true;
#endif
#ifdef LUMEN_PLATFORM_MACOS
    has_platform = true;
#endif
#ifdef LUMEN_PLATFORM_LINUX
    has_platform = true;
#endif
#ifdef LUMEN_PLATFORM_ANDROID
    has_platform = true;
#endif
#ifdef LUMEN_PLATFORM_IOS
    has_platform = true;
#endif
    
    EXPECT_TRUE(has_platform);
}