#include <matching_engine_core/matching_engine.hpp>
#include <matching_engine_core/fixed_pool_allocator.hpp>
#include <matching_engine_core/huge_page.hpp>

#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

namespace {

using matching_engine::AddOrderRequest;
using matching_engine::CancelOrderRequest;
using matching_engine::FixedPoolAllocator;
using matching_engine::MatchingEngine;
using matching_engine::ProcessResult;
using matching_engine::Side;

std::vector<std::string> outputs_to_csv(const ProcessResult& result) {
    std::vector<std::string> values;
    values.reserve(result.outputs.size());
    for (const auto& msg : result.outputs) {
        values.push_back(msg.to_csv());
    }
    return values;
}

class MatchingEngineCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine.reset();
    }

    MatchingEngine engine;
};

TEST(FixedPoolAllocatorTest, ReusesFixedFlatStorageOneObjectAtATime) {
    FixedPoolAllocator<int, 2> allocator;
    int* first = std::allocator_traits<decltype(allocator)>::allocate(allocator, 1U);
    int* second = std::allocator_traits<decltype(allocator)>::allocate(allocator, 1U);

    EXPECT_THROW((void)std::allocator_traits<decltype(allocator)>::allocate(allocator, 1U),
                 std::bad_alloc);
    EXPECT_THROW((void)std::allocator_traits<decltype(allocator)>::allocate(allocator, 2U),
                 std::bad_alloc);

    std::allocator_traits<decltype(allocator)>::deallocate(allocator, first, 1U);
    int* reused = std::allocator_traits<decltype(allocator)>::allocate(allocator, 1U);
    EXPECT_EQ(reused, first);

    std::allocator_traits<decltype(allocator)>::deallocate(allocator, second, 1U);
    std::allocator_traits<decltype(allocator)>::deallocate(allocator, reused, 1U);
}

TEST(FixedPoolAllocatorTest, SupportsContiguousAllocationForVectorGrowth) {
    std::vector<int, FixedPoolAllocator<int, 8>> values;
    values.push_back(10);
    values.push_back(20);
    values.push_back(30);

    EXPECT_EQ((std::vector<int>{values.begin(), values.end()}),
              (std::vector<int>{10, 20, 30}));
}

TEST(FixedPoolAllocatorTest, SupportsStlNodeContainerAllocation) {
    std::list<int, FixedPoolAllocator<int, 3>> values;
    values.push_back(10);
    values.push_back(20);
    values.push_back(30);

    EXPECT_EQ((std::vector<int>{values.begin(), values.end()}),
              (std::vector<int>{10, 20, 30}));
    EXPECT_THROW(values.push_back(40), std::bad_alloc);
}

TEST(HugePageReservationTest, ReportsAConsistentStartupReservationStatus) {
    const auto reservation = matching_engine::huge_page_reservation();
    EXPECT_EQ(reservation.size_bytes == 0U, !reservation.is_reserved);
}

TEST_F(MatchingEngineCoreTest, RejectsInvalidAddRequests) {
    EXPECT_EQ(engine.handle_add_request(AddOrderRequest{0, Side::Buy, 1, 100.0L}).errors[0],
              "Invalid AddOrderRequest orderid");
    EXPECT_EQ(engine.handle_add_request(AddOrderRequest{1, Side::Buy, 0, 100.0L}).errors[0],
              "Invalid AddOrderRequest quantity");
    EXPECT_EQ(engine.handle_add_request(AddOrderRequest{1, Side::Buy, 1, 0.0L}).errors[0],
              "Invalid AddOrderRequest price");
}

TEST_F(MatchingEngineCoreTest, RejectsInvalidCancelRequests) {
    EXPECT_EQ(engine.handle_cancel_request(CancelOrderRequest{0}).errors[0],
              "Invalid CancelOrderRequest orderid");
    EXPECT_EQ(engine.handle_cancel_request(CancelOrderRequest{999}).errors[0],
              "Unknown orderid for cancel: 999");
}

TEST_F(MatchingEngineCoreTest, DetectsDuplicateOrderId) {
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{100, Side::Buy, 5, 100.0L}).errors.empty());

    const auto dup = engine.handle_add_request(AddOrderRequest{100, Side::Sell, 4, 99.0L});
    ASSERT_EQ(dup.errors.size(), 1U);
    EXPECT_EQ(dup.errors[0], "Duplicate orderid: 100");
}

TEST_F(MatchingEngineCoreTest, CancelsRestingOrderSuccessfully) {
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{200, Side::Buy, 5, 100.0L}).errors.empty());

    const auto cancel_ok = engine.handle_cancel_request(CancelOrderRequest{200});
    EXPECT_TRUE(cancel_ok.errors.empty());
    EXPECT_TRUE(cancel_ok.outputs.empty());

    const auto cancel_again = engine.handle_cancel_request(CancelOrderRequest{200});
    ASSERT_EQ(cancel_again.errors.size(), 1U);
    EXPECT_EQ(cancel_again.errors[0], "Unknown orderid for cancel: 200");
}

TEST_F(MatchingEngineCoreTest, MatchesAtRestingOrderPrice) {
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{300, Side::Sell, 5, 101.0L}).errors.empty());

    const auto buy = engine.handle_add_request(AddOrderRequest{301, Side::Buy, 2, 105.0L});
    EXPECT_TRUE(buy.errors.empty());
    EXPECT_EQ(outputs_to_csv(buy), (std::vector<std::string>{"2,2,101", "3,301", "4,300,3"}));
}

TEST_F(MatchingEngineCoreTest, PricePriorityAcrossLevels) {
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{400, Side::Sell, 4, 101.0L}).errors.empty());
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{401, Side::Sell, 4, 100.0L}).errors.empty());

    const auto buy = engine.handle_add_request(AddOrderRequest{402, Side::Buy, 6, 101.0L});
    EXPECT_TRUE(buy.errors.empty());

    EXPECT_EQ(outputs_to_csv(buy),
              (std::vector<std::string>{
                  "2,4,100", "4,402,2", "3,401",
                  "2,2,101", "3,402", "4,400,2",
              }));
}

TEST_F(MatchingEngineCoreTest, TimePriorityWithinSamePriceLevel) {
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{500, Side::Sell, 2, 100.0L}).errors.empty());
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{501, Side::Sell, 2, 100.0L}).errors.empty());

    const auto buy = engine.handle_add_request(AddOrderRequest{502, Side::Buy, 3, 100.0L});
    EXPECT_TRUE(buy.errors.empty());

    EXPECT_EQ(outputs_to_csv(buy),
              (std::vector<std::string>{
                  "2,2,100", "4,502,1", "3,500",
                  "2,1,100", "3,502", "4,501,1",
              }));
}

TEST_F(MatchingEngineCoreTest, SellAggressorMatchesBestBidFirst) {
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{600, Side::Buy, 3, 99.0L}).errors.empty());
    EXPECT_TRUE(engine.handle_add_request(AddOrderRequest{601, Side::Buy, 2, 100.0L}).errors.empty());

    const auto sell = engine.handle_add_request(AddOrderRequest{602, Side::Sell, 4, 99.0L});
    EXPECT_TRUE(sell.errors.empty());

    EXPECT_EQ(outputs_to_csv(sell),
              (std::vector<std::string>{
                  "2,2,100", "4,602,2", "3,601",
                  "2,2,99", "3,602", "4,600,1",
              }));
}

}  // namespace
