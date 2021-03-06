// (c) 2013 Maciej Gajewski, <maciej.gajewski0@gmail.com>
#include "coroutines/globals.hpp"

#include "test/fixtures.hpp"

#include <boost/test/unit_test.hpp>

namespace coroutines { namespace tests {

// simple reader-wrtier test, wrtier closes before reader finishes
// expected: reader will read all data, and then throw channel_closed
BOOST_FIXTURE_TEST_CASE(test_reading_after_close, fixture)
{

    // create channel
    channel_pair<int> pair = make_channel<int>(10);

    int last_written = -1;
    int last_read = -1;

    // writer coroutine
    go(std::string("reading_after_close writer"), [&last_written](channel_writer<int>& writer)
    {
        for(int i = 0; i < 5; i++)
        {
            writer.put(i);
            last_written = i;
        }
    }, std::move(pair.writer));

    // reader coroutine
    go(std::string("reading_after_close reader"), [&last_read](channel_reader<int>& reader)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for(;;)
        {
            try
            {
                last_read = reader.get();
            }
            catch(const channel_closed&)
            {
                break;
            }
        }
    }, std::move(pair.reader));

    wait_for_completion();

    BOOST_CHECK_EQUAL(last_written, 4);
    BOOST_CHECK_EQUAL(last_read, 4);
}

// Reader blocking:  reader should block until wrtier writes
BOOST_FIXTURE_TEST_CASE(test_reader_blocking, fixture)
{
    // create channel
    channel_pair<int> pair = make_channel<int>(10);

    bool reader_finished = false;
    go(std::string("test_reader_blocking reader"), [&reader_finished](channel_reader<int>& r)
    {
        r.get();
        reader_finished = true;
    }, std::move(pair.reader));

    BOOST_CHECK_EQUAL(reader_finished, false);

    bool writer_finished = false;
    go(std::string("test_reader_blocking writer"), [&writer_finished](channel_writer<int>& w)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        w.put(7);
        writer_finished = true;

    }, std::move(pair.writer));

    wait_for_completion();

    BOOST_CHECK_EQUAL(reader_finished, true);
    BOOST_CHECK_EQUAL(writer_finished, true);
}

// test - writer.put() should exit with exception if reader closes channel
BOOST_FIXTURE_TEST_CASE(test_writer_exit_when_closed, fixture)
{
    // create channel
    channel_pair<int> pair = make_channel<int>(1);


    go(std::string("test_writer_exit_when_closed reader"), [](channel_reader<int>& r)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // do nothing, close the chanel on exit
    }, std::move(pair.reader));

    bool writer_threw = false;
    go(std::string("test_writer_exit_when_closed writer"), [&writer_threw](channel_writer<int>& w)
    {
        try
        {
            w.put(1);
            w.put(2); // this will block
        }
        catch(const channel_closed&)
        {
            writer_threw = true;
        }

    }, std::move(pair.writer));

    wait_for_completion();

    BOOST_CHECK_EQUAL(writer_threw, true);
}

// send more items than channels capacity
BOOST_FIXTURE_TEST_CASE(test_large_transfer, fixture)
{
    // create channel
    channel_pair<int> pair = make_channel<int>(10);

    int last_written = -1;
    int last_read = -1;
    static const int MESSAGES = 10000;

    // writer coroutine
    go(std::string("large_transfer writer"), [&last_written](channel_writer<int>& writer)
    {
        for(int i = 0; i < MESSAGES; i++)
        {
            writer.put(i);
            last_written = i;
            if ((i % 37) == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                BOOST_TEST_MESSAGE("long write progress: " << i << "/" << MESSAGES);
            }
        }
    }, std::move(pair.writer));

    // reader coroutine
    go(std::string("large_transfer reader"), [&last_read](channel_reader<int>& reader)
    {
        for(int i = 0;;i++)
        {
            try
            {
                last_read = reader.get();
                if ((i % 53) == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    BOOST_TEST_MESSAGE("long read progress: " << i << "/" << MESSAGES);
                }
            }
            catch(const channel_closed&)
            {
                break;
            }
        }
    }, std::move(pair.reader));

    wait_for_completion();

    BOOST_CHECK_EQUAL(last_written, MESSAGES-1);
    BOOST_CHECK_EQUAL(last_read, MESSAGES-1);
}

BOOST_FIXTURE_TEST_CASE(test_multiple_readers, fixture)
{
    static const int MSGS = 10000;
    static const int READERS = 100;
    std::atomic<int> received(0);
    std::atomic<int> closed(0);

    channel_pair<int> pair = make_channel<int>(10);

    go("test_multiple_readers writer", [](channel_writer<int>& writer)
    {
        for(int i = 0; i < MSGS; i++)
        {
            writer.put(i);
        }
    }, std::move(pair.writer));

    for(int i = 0; i < READERS; i++)
    {
        go("test_multiple_readers reader", [&received, &closed](channel_reader<int> reader)
        {
            try
            {
                for(;;)
                {
                    reader.get();
                    received++;
                }
            }
            catch(const channel_closed&)
            {
                closed++;
                throw;
            }

        }, pair.reader);
    }

    wait_for_completion();

    BOOST_CHECK_EQUAL(closed, READERS);
    BOOST_CHECK_EQUAL(received, MSGS);
}

BOOST_FIXTURE_TEST_CASE(test_multiple_writers, fixture)
{
    static const int MSGS_PER_WRITER = 100;
    static const int WRITERS = 100;

    std::atomic<int> received(0);

    channel_pair<int> pair = make_channel<int>(10);

    for(int i = 0; i < WRITERS; i++)
    {
        go(
            boost::str(boost::format("test_multiple_readers writer %d") % i),
            [](channel_writer<int> writer)
        {
            for(int i = 0; i < MSGS_PER_WRITER; i++)
            {
                writer.put(i);
            }
        }, pair.writer);
    }
    pair.writer.close();

    go("test_multiple_readers reader", [&received](channel_reader<int>& reader)
    {
        for(;;)
        {
            reader.get();
            received++;
        }

    }, std::move(pair.reader));

    wait_for_completion();

    BOOST_CHECK_EQUAL(received, WRITERS*MSGS_PER_WRITER);
}

BOOST_FIXTURE_TEST_CASE(test_non_blocking_read, fixture)
{
    auto pair = make_channel<double>(10);
    int read = 0;
    bool completed = false;

    go([&]()
    {
        pair.writer.put(1.1);
        pair.writer.put(2.2);
        pair.writer.put(3.3);

        double x;
        while(pair.reader.try_get(x))
            read++;
        completed = true;
    });

    wait_for_completion();

    BOOST_CHECK_EQUAL(read, 3);
    BOOST_CHECK_EQUAL(completed, true);
}

}}
