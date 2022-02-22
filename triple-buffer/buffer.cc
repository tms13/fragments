#include <gtest/gtest.h>

// test_invariant() member function is enabled by including after TEST is defined.
#include "buffer.hh"

#include <set>


#define EXPECT_INVARIANT(obj) (obj.test_invariant(__FILE__, __LINE__))


TEST(triple_buffer, no_write_read_returns_null)
{
    triple_buffer<int> buffer;
    EXPECT_INVARIANT(buffer);
    EXPECT_EQ(buffer.get_read_buffer({}), nullptr);
    EXPECT_INVARIANT(buffer);
}

TEST(triple_buffer, write_once_read_once)
{
    triple_buffer<int> buffer;
    auto *write0 = buffer.get_write_buffer();
    EXPECT_INVARIANT(buffer);
    EXPECT_NE(write0, nullptr);
    EXPECT_EQ(buffer.get_read_buffer({}), nullptr);
    EXPECT_INVARIANT(buffer);

    buffer.set_write_complete();
    EXPECT_INVARIANT(buffer);
    // having written, we can read
    EXPECT_EQ(buffer.get_read_buffer({}), write0);
    EXPECT_INVARIANT(buffer);

    // another read should block/fail
    EXPECT_EQ(buffer.get_read_buffer({}), nullptr);
    EXPECT_INVARIANT(buffer);
}

TEST(triple_buffer, write_twice_read)
{
    triple_buffer<int> buffer;
    auto *write0 = buffer.get_write_buffer();
    EXPECT_NE(write0, nullptr);
    buffer.set_write_complete();

    auto *write1 = buffer.get_write_buffer();
    EXPECT_NE(write1, nullptr);
    EXPECT_NE(write1, write0);
    buffer.set_write_complete();

    // read should get newest
    EXPECT_EQ(buffer.get_read_buffer({}), write1);
    // another read should block/fail
    EXPECT_EQ(buffer.get_read_buffer({}), nullptr);
}

TEST(triple_buffer, write_read_write2)
{
    triple_buffer<int> buffer;
    auto *write0 = buffer.get_write_buffer();
    EXPECT_NE(write0, nullptr);
    buffer.set_write_complete();

    // read should get newest
    auto *read0 = buffer.get_read_buffer({});
    EXPECT_EQ(read0, write0);

    auto *write1 = buffer.get_write_buffer();
    EXPECT_NE(write1, nullptr);
    EXPECT_NE(write1, write0);
    buffer.set_write_complete();

    auto *write2 = buffer.get_write_buffer();
    EXPECT_NE(write2, nullptr);
    EXPECT_NE(write2, write1);
    EXPECT_NE(write2, read0);   // don't touch reader's buffer
    buffer.set_write_complete();

    // read should get newest
    auto *read1 = buffer.get_read_buffer({});
    EXPECT_EQ(read1, write2);
}

TEST(triple_buffer, write_read_write3)
{
    triple_buffer<int> buffer;
    auto *write0 = buffer.get_write_buffer();
    EXPECT_NE(write0, nullptr);
    buffer.set_write_complete();

    // read should get newest
    auto *read0 = buffer.get_read_buffer({});
    EXPECT_EQ(read0, write0);

    auto *write1 = buffer.get_write_buffer();
    EXPECT_NE(write1, nullptr);
    EXPECT_NE(write1, write0);
    buffer.set_write_complete();

    auto *write2 = buffer.get_write_buffer();
    EXPECT_NE(write2, nullptr);
    EXPECT_NE(write2, write1);
    EXPECT_NE(write2, read0);   // don't touch reader's buffer
    buffer.set_write_complete();

    auto *write3 = buffer.get_write_buffer();
    EXPECT_EQ(write3, write1);  // we should be overwriting the old unread value
    EXPECT_NE(write3, read0);   // still don't touch reader's buffer
    buffer.set_write_complete();

    // read should get newest
    auto *read1 = buffer.get_read_buffer({});
    EXPECT_EQ(read1, write3);
}


TEST(triple_buffer, read_during_write)
{
    triple_buffer<int> buffer;
    auto *write0 = buffer.get_write_buffer();
    EXPECT_NE(write0, nullptr);
    buffer.set_write_complete();

    auto *write1 = buffer.get_write_buffer();

    // read should get complete buffer
    auto *read0 = buffer.get_read_buffer({});
    EXPECT_EQ(read0, write0);

    buffer.set_write_complete();
    auto *write2 = buffer.get_write_buffer();
    EXPECT_NE(write2, nullptr);
    EXPECT_NE(write2, write1);
    EXPECT_NE(write2, read0);   // don't touch reader's buffer

    auto *read1 = buffer.get_read_buffer({});
    EXPECT_EQ(read1, write1);

    buffer.set_write_complete();
}

