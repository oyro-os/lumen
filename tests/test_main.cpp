#include <gtest/gtest.h>
#include <lumen/lumen.h>

class LumenTestEnvironment : public ::testing::Environment {
   public:
    void SetUp() override {
        ASSERT_EQ(LUMEN_OK, lumen_initialize());
    }

    void TearDown() override {
        lumen_shutdown();
    }
};

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new LumenTestEnvironment());
    return RUN_ALL_TESTS();
}