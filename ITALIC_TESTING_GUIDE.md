# Testing Italic Formatting in the Simulator

This guide explains how to test the newly implemented italic formatting support using the Sol Reader simulator.

## Quick Start

### 1. Build and Run the Simulator
```bash
make sim
```

The simulator will start and display the home screen. Watch the log output in `logs/sim.log`.

### 2. Load the Italic Test EPUB
The test EPUB has been created at:
```
test/CatchTests/Fixtures/epub/italic-test.epub
```

This EPUB contains comprehensive italic formatting tests with:
- Basic `<i>`, `<em>`, and `<strong>` tags
- Multiple italic regions in paragraphs
- Edge cases (single characters, word breaks, adjacent regions)
- Reading content with mixed formatting

## Testing Approaches

### Approach A: Direct Unit Tests (Recommended for Development)

Run the italic-specific tests:

```bash
# Build and run all tests
make test

# Run only italic-related tests (if tags are added)
make test-build && ./test/CatchTests/build/tests '[italic]'
```

This will validate:
- Token creation with italic state
- Pager segmentation at italic boundaries
- Rendering pipeline integration

### Approach B: Manual Simulator Testing

#### Method 1: Using Test Data

Test EPUBs are located in:
```
test/CatchTests/Fixtures/epub/
```

Available files:
- `italic-test.epub` ← **NEW: Comprehensive italic tests**
- `formatting.epub` ← Existing formatting tests
- `alchemyst.epub` ← Large book for integration testing

#### Method 2: Adding Your Own EPUB

1. **Copy your EPUB** to `test/CatchTests/Fixtures/epub/`
   ```bash
   cp /path/to/your/book.epub test/CatchTests/Fixtures/epub/your-book.epub
   ```

2. **Update the simulator preferences** (`simprefs.ini`) to load it:
   ```ini
   [prefs]
   # Point to your book
   # Format: d7570c42-po=[EPUB\;offset:0,href:content.xhtml]
   ```

#### Method 3: Creating Test EPUBs Programmatically

A Python script is available to generate test EPUBs:
```bash
python3 test/CatchTests/Fixtures/epub/create_glyphs_epub.py
```

## What to Look For When Testing

### Visual Indicators of Successful Italic Rendering

1. **Text Slant**: Italic text should appear slanted to the right (approximately 11-15 degree angle)
2. **Consistent Style**: All `<i>`, `<em>`, and `<strong>` tags should render consistently
3. **Proper Boundaries**: Regular text before/after italics should render normally
4. **Text Flow**: Italic text should wrap naturally at line breaks without artifacts
5. **Reading Comfort**: No visual glitches or overlapping characters

### Performance Indicators

- Frame rate should remain stable during italic rendering
- No memory leaks or allocation errors in logs
- Scrolling through italic text should be smooth

### Potential Issues to Watch For

| Issue | Expected Behavior | Debug Step |
|-------|-------------------|-----------|
| Italic not rendering | Text appears italic-styled | Check `logs/sim.log` for token creation |
| Incorrect angle | Text should slant ~15° right | Adjust skew factor in TTFFont.cpp line 505 |
| Missing boundaries | Italics should start/stop cleanly | Verify Pager.cpp segment breaking logic |
| Performance drop | Frame rate should stay ≥10 FPS | Profile with `make test` performance metrics |
| Text overlap | No overlapping characters | Check horizontal offset calculation |

## Debugging Steps

### 1. Check Build Status
```bash
. ~/esp/esp-idf/export.sh && make build
# or for simulator:
make sim-build
```

### 2. Watch Simulator Logs in Real-Time
```bash
# Terminal 1: Run simulator
make sim

# Terminal 2: Watch logs
tail -f logs/sim.log
```

### 3. Verify Italic State in Code

Look for these markers in debug output:

```
EPubTokenizer::interpret: Processing Tag::I (italic enabled)
EPubTokenizer::interpret: Processing Tag::EM (italic enabled)
Pager::getLineSegment: Breaking at italic state change
Pager::drawSingleLine: Applying italic transform
```

### 4. Add Temporary Debug Logging

In [components/tiny-epub/renderer/src/EPubTokenizer.cpp](/Users/greg/source/embedded-app/components/tiny-epub/renderer/src/EPubTokenizer.cpp):

```cpp
case Tag::I:
case Tag::EM:
case Tag::STRONG:
    log_d("Italic tag encountered: %s", tokenizingStarted_ ? "active" : "pending");
    italic_ = true;
    break;
```

In [components/tiny-epub/renderer/src/Pager.cpp](/Users/greg/source/embedded-app/components/tiny-epub/renderer/src/Pager.cpp):

```cpp
if (italic != segment_italic_) {
    log_d("Italic state transition: %d -> %d", segment_italic_, italic);
    break;
}
```

## File Locations for Reference

### Core Implementation Files
- **Token Definition**: [components/tiny-epub/renderer/include/Renderer/RendererDefs.hpp](/Users/greg/source/embedded-app/components/tiny-epub/renderer/include/Renderer/RendererDefs.hpp)
- **HTML Tag Parsing**: [components/tiny-epub/renderer/src/EPubTokenizer.cpp](/Users/greg/source/embedded-app/components/tiny-epub/renderer/src/EPubTokenizer.cpp)
- **Text Segmentation**: [components/tiny-epub/renderer/src/Pager.cpp](/Users/greg/source/embedded-app/components/tiny-epub/renderer/src/Pager.cpp)
- **Italic Transformation**: [components/tiny-font/src/TTFDriver/TTFFont.cpp](/Users/greg/source/embedded-app/components/tiny-font/src/TTFDriver/TTFFont.cpp)
- **System Integration**: [src/Displays/DisplaySystem.hpp](/Users/greg/source/embedded-app/src/Displays/DisplaySystem.hpp)

### Test Resources
- **Test EPUB**: [test/CatchTests/Fixtures/epub/italic-test.epub](/Users/greg/source/embedded-app/test/CatchTests/Fixtures/epub/italic-test.epub)
- **Other Test EPUBs**: [test/CatchTests/Fixtures/epub/](/Users/greg/source/embedded-app/test/CatchTests/Fixtures/epub/)
- **Test Data**: [test/CatchTests/TestData/](/Users/greg/source/embedded-app/test/CatchTests/TestData/)

## Tuning the Italic Angle

If the italic angle doesn't look right, you can adjust it in [components/tiny-font/src/TTFDriver/TTFFont.cpp](/Users/greg/source/embedded-app/components/tiny-font/src/TTFDriver/TTFFont.cpp) at line 504:

```cpp
// Current formula: offset = yoff * 0.2 (20% of vertical offset)
// Try these alternatives:
outPos.x += (glyph.value()->metrics.yoff * 15) / 100;  // 15% (less slant)
outPos.x += (glyph.value()->metrics.yoff * 25) / 100;  // 25% (more slant)
```

Then rebuild and test:
```bash
. ~/esp/esp-idf/export.sh && make clang-tidy && make sim
```

## Success Criteria

Your implementation is working correctly when:

✅ Simulator builds without errors  
✅ Test EPUBs load successfully  
✅ `<i>` tags render with italic styling  
✅ `<em>` tags render with italic styling  
✅ `<strong>` tags render with italic styling  
✅ Text boundaries are clean (no overlap)  
✅ Line wrapping works correctly  
✅ Performance remains stable  
✅ All unit tests pass  

## Next Steps

1. **Visual Validation**: Run the simulator and visually inspect italic rendering
2. **Performance Testing**: Profile frame rates during italic text rendering
3. **Edge Cases**: Test with various EPUBs containing italic content
4. **Fine-tuning**: Adjust the italic skew angle if needed for optimal appearance

## Questions or Issues?

- Check the comprehensive change summary: [ITALIC_FEATURE_COMPLETE.md](ITALIC_FEATURE_COMPLETE.md)
- Review implementation details: [italic formatting code](#file-locations-for-reference)
- Verify compilation: `. ~/esp/esp-idf/export.sh && make build`

---

**Last Updated**: October 29, 2026  
**Implementation Status**: ✅ Complete and Ready for Testing
