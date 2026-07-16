#include <matching_engine_core/line_matching_engine.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using matching_engine::LineMatchingEngine;
using matching_engine::ProcessResult;

std::vector<std::string> outputs_to_csv(const ProcessResult& result) {
    std::vector<std::string> values;
    values.reserve(result.outputs.size());
    for (const auto& msg : result.outputs) {
        values.push_back(msg.to_csv());
    }
    return values;
}

class LineMatchingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine.reset();
    }

    LineMatchingEngine engine;
};

TEST_F(LineMatchingEngineTest, IgnoresEmptyAndCommentLines) {
    const auto blank = engine.process_line("   ");
    EXPECT_TRUE(blank.outputs.empty());
    EXPECT_TRUE(blank.errors.empty());

    const auto comment_only = engine.process_line("// only comment");
    EXPECT_TRUE(comment_only.outputs.empty());
    EXPECT_TRUE(comment_only.errors.empty());
}

TEST_F(LineMatchingEngineTest, RejectsUnknownMessageType) {
    const auto bad_text = engine.process_line("foo,1,2");
    ASSERT_EQ(bad_text.errors.size(), 1U);
    EXPECT_EQ(bad_text.errors[0], "Unknown message type: foo");

    const auto bad_num = engine.process_line("9,1,2");
    ASSERT_EQ(bad_num.errors.size(), 1U);
    EXPECT_EQ(bad_num.errors[0], "Unknown message type: 9");
}

TEST_F(LineMatchingEngineTest, ValidatesAddAndCancelFormats) {
    EXPECT_EQ(engine.process_line("0,1,0,5").errors[0], "AddOrderRequest expects 5 fields");
    EXPECT_EQ(engine.process_line("0,0,0,5,100").errors[0], "Invalid AddOrderRequest orderid");
    EXPECT_EQ(engine.process_line("0,1,2,5,100").errors[0], "Invalid AddOrderRequest side");
    EXPECT_EQ(engine.process_line("0,1,0,0,100").errors[0], "Invalid AddOrderRequest quantity");
    EXPECT_EQ(engine.process_line("0,1,0,5,0").errors[0], "Invalid AddOrderRequest price");

    EXPECT_EQ(engine.process_line("1").errors[0], "CancelOrderRequest expects 2 fields");
    EXPECT_EQ(engine.process_line("1,0").errors[0], "Invalid CancelOrderRequest orderid");
}

TEST_F(LineMatchingEngineTest, ParsesAndRoutesToMatchingEngine) {
    EXPECT_TRUE(engine.process_line("0,100,1,5,101 // resting ask").errors.empty());

    const auto buy = engine.process_line("0,101,0,2,105");
    EXPECT_TRUE(buy.errors.empty());
    EXPECT_EQ(outputs_to_csv(buy), (std::vector<std::string>{"2,2,101", "3,101", "4,100,3"}));
}

TEST_F(LineMatchingEngineTest, CancelFlowThroughLineInterface) {
    EXPECT_TRUE(engine.process_line("0,200,0,5,100").errors.empty());
    EXPECT_TRUE(engine.process_line("1,200").errors.empty());

    const auto cancel_again = engine.process_line("1,200");
    ASSERT_EQ(cancel_again.errors.size(), 1U);
    EXPECT_EQ(cancel_again.errors[0], "Unknown orderid for cancel: 200");
}

}  // namespace
