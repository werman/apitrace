/**************************************************************************
 *
 * Copyright 2014 LunarG, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include "trace_parser.hpp"


namespace trace {


// Decorator for parser which loops
class LastFrameLoopParser : public AbstractParser  {
public:
    LastFrameLoopParser(AbstractParser *p, int c) :
        requestedLoopCount(c), loopCount(c), parser(p) {}

    ~LastFrameLoopParser() {
        clearCache();
        delete parser;
    }

    Call *parse_call(void) override;

    // Delegate to Parser
    void getBookmark(ParseBookmark &bookmark) override { parser->getBookmark(bookmark); }
    void setBookmark(const ParseBookmark &bookmark) override { parser->setBookmark(bookmark); }
    bool open(const char *filename) override;
    void close(void) override;
    unsigned long long getVersion(void) const override { return parser->getVersion(); }
    const Properties & getProperties(void) const override { return parser->getProperties(); }
private:
    const int requestedLoopCount;
    int loopCount;
    AbstractParser *parser;
    size_t callIndex = 0;
    size_t finalFrameStart = 0;
    size_t loopIndex = 0;
    bool atEnd = false;
    std::vector<Call *> lastFrameCalls;
    void clearCache();
};


bool
LastFrameLoopParser::open(const char *filename)
{
    close();
    loopCount = requestedLoopCount;
    callIndex = finalFrameStart = loopIndex = 0;
    atEnd = false;

    if (loopCount) {
        // The last frame cannot be identified until EOF. Scan first without
        // allocating argument blobs, then stream all preceding calls normally.
        // Do not use bookmarks: Brotli and gzip traces are not seekable.
        Parser scanner;
        if (!scanner.open(filename)) {
            return false;
        }
        size_t index = 0;
        bool startsNewFrame = true;
        while (Call *call = scanner.scan_call()) {
            if (startsNewFrame) {
                finalFrameStart = index;
            }
            startsNewFrame = call->flags & CALL_FLAG_END_FRAME;
            ++index;
            delete call;
        }
    }

    return parser->open(filename);
}

void
LastFrameLoopParser::clearCache()
{
    for (auto call : lastFrameCalls) {
        delete call;
    }
    lastFrameCalls.clear();
}

void
LastFrameLoopParser::close()
{
    clearCache();
    parser->close();
}

Call *
LastFrameLoopParser::parse_call(void)
{
    if (!atEnd) {
        if (Call *call = parser->parse_call()) {
            if (loopCount && callIndex >= finalFrameStart) {
                call->reuse_call = true;
                lastFrameCalls.push_back(call);
            }
            ++callIndex;
            return call;
        }
        atEnd = true;
    }

    if (!loopCount || lastFrameCalls.empty()) {
        return nullptr;
    }

    Call *call = lastFrameCalls[loopIndex++];
    if (loopIndex == lastFrameCalls.size()) {
        loopIndex = 0;
        if (loopCount > 0) {
            --loopCount;
        }
    }

    return call;
}


AbstractParser *
lastFrameLoopParser(AbstractParser *parser, int loopCount)
{
    return new LastFrameLoopParser(parser, loopCount);
}


} /* namespace trace */
