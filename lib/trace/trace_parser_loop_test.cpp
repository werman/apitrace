#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>
#include <brotli/encode.h>
#include <zlib.h>

#include "trace_file.hpp"
#include "trace_format.hpp"
#include "trace_parser.hpp"
#include "trace_writer.hpp"
#include "../../retrace/retrace_call_filter.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

class LoopParserTest : public testing::TestWithParam<int> {
protected:
    std::string path;
    std::unique_ptr<trace::AbstractParser> parser;

    void SetUp() override {
#ifdef _WIN32
        char directory[MAX_PATH], filename[MAX_PATH];
        ASSERT_NE(GetTempPathA(MAX_PATH, directory), 0u);
        ASSERT_NE(GetTempFileNameA(directory, "apt", 0, filename), 0u);
#else
        char filename[] = "/tmp/apitrace-loop-XXXXXX";
        int fd = mkstemp(filename);
        ASSERT_NE(fd, -1);
        close(fd);
#endif
        path = filename;
    }

    void TearDown() override {
        parser.reset();
        std::remove(path.c_str());
    }

    void write(const std::vector<bool> &presents) {
        static const char *argNames[] = {"data"};
        static const trace::FunctionSig upload = {0, "memcpy", 1, argNames};
        static const trace::FunctionSig present = {1, "IDirect3DDevice9::Present", 0, nullptr};
        trace::Writer writer;
        ASSERT_TRUE(writer.open(path.c_str(), TRACE_VERSION, {}));
        for (size_t i = 0; i < presents.size(); ++i) {
            trace::Call call(presents[i] ? &present : &upload, trace::CallFlags(0), i % 2);
            if (!presents[i]) {
                auto blob = new trace::Blob(65536);
                std::memset(blob->buf, static_cast<int>(i), blob->size);
                call.args[0].value = blob;
            }
            writer.writeCall(&call);
        }
        writer.close();
        if (GetParam() == 0) {
            return;
        }
        // Convert the Snappy fixture to non-seekable gzip/Brotli. The loop
        // implementation must not require random access to either format.
        std::unique_ptr<trace::File> file(trace::File::createForRead(path.c_str()));
        ASSERT_TRUE(file);
        std::vector<unsigned char> raw;
        unsigned char chunk[4096];
        size_t count;
        while ((count = file->read(chunk, sizeof chunk)) != 0) {
            raw.insert(raw.end(), chunk, chunk + count);
        }
        file.reset();
        if (GetParam() == 1) {
            gzFile gzip = gzopen(path.c_str(), "wb");
            ASSERT_NE(gzip, nullptr);
            EXPECT_EQ(gzwrite(gzip, raw.data(), static_cast<unsigned>(raw.size())),
                      static_cast<int>(raw.size()));
            EXPECT_EQ(gzclose(gzip), Z_OK);
        } else {
            size_t length = BrotliEncoderMaxCompressedSize(raw.size());
            std::vector<unsigned char> encoded(length);
            ASSERT_TRUE(BrotliEncoderCompress(1, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_GENERIC,
                                             raw.size(), raw.data(), &length, encoded.data()));
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char *>(encoded.data()), length);
            ASSERT_TRUE(output.good());
        }
    }

    void open(int loops) {
        parser.reset(trace::lastFrameLoopParser(new trace::Parser, loops));
        ASSERT_TRUE(parser->open(path.c_str()));
    }

    void expectCall(unsigned number, bool retained) {
        trace::Call *call = parser->parse_call();
        ASSERT_NE(call, nullptr);
        EXPECT_EQ(call->no, number);
        EXPECT_EQ(call->thread_id, number % 2);
        EXPECT_EQ(call->reuse_call, retained);
        if (!call->reuse_call) {
            delete call;
        }
    }
};

TEST_P(LoopParserTest, RetainsOnlyFinalFrameAndPreservesFiniteLoopCount) {
    write({false, true, false, true});
    open(2);
    expectCall(0, false);
    expectCall(1, false);
    trace::Call *first = parser->parse_call();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->no, 2u);
    EXPECT_TRUE(first->reuse_call);
    expectCall(3, true);
    for (int loop = 0; loop < 2; ++loop) {
        EXPECT_EQ(parser->parse_call(), first);
        expectCall(3, true);
    }
    EXPECT_EQ(parser->parse_call(), nullptr);
}

TEST_P(LoopParserTest, NoLoopsNeverRetainsCalls) {
    write({false, true, false, true});
    open(0);
    for (unsigned i = 0; i < 4; ++i) expectCall(i, false);
    EXPECT_EQ(parser->parse_call(), nullptr);
}

TEST_P(LoopParserTest, TrailingCallsRemainTheFinalFrame) {
    write({false, true, false});
    open(1);
    expectCall(0, false);
    expectCall(1, false);
    expectCall(2, true);
    expectCall(2, true);
    EXPECT_EQ(parser->parse_call(), nullptr);
}

TEST_P(LoopParserTest, EmptyTraceDoesNotIndexEmptyCache) {
    write({});
    open(-1);
    EXPECT_EQ(parser->parse_call(), nullptr);
}

TEST_P(LoopParserTest, InfiniteLoopsAndReopenResetState) {
    write({false, true});
    open(-1);
    for (unsigned i = 0; i < 20; ++i) expectCall(i % 2, true);
    ASSERT_TRUE(parser->open(path.c_str()));
    expectCall(0, true);
    expectCall(1, true);
}

TEST_P(LoopParserTest, ReopenRestoresFiniteLoopBudget) {
    write({false});
    open(1);
    expectCall(0, true);
    expectCall(0, true);
    EXPECT_EQ(parser->parse_call(), nullptr);
    ASSERT_TRUE(parser->open(path.c_str()));
    expectCall(0, true);
    expectCall(0, true);
    EXPECT_EQ(parser->parse_call(), nullptr);
}

INSTANTIATE_TEST_SUITE_P(Compression, LoopParserTest, testing::Values(0, 1, 2));

TEST(CursorCallFilter, MatchesOnlyD3DCursorMethods) {
    for (const char *interfaceName : {"IDirect3DDevice8::", "IDirect3DDevice9::", "IDirect3DDevice9Ex::"}) {
        for (const char *method : {"SetCursorProperties", "SetCursorPosition", "ShowCursor"}) {
            EXPECT_TRUE(retrace::isD3DCursorCall((std::string(interfaceName) + method).c_str()));
        }
    }
    for (const char *name : {"IDirect3DDevice9::Present", "IDirect3DDevice9::ShowCursorExtra",
                             "IDirect3DDevice9::SetViewport", "Other::SetCursorPosition", "ShowCursor"}) {
        EXPECT_FALSE(retrace::isD3DCursorCall(name));
    }
}

} // namespace

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
