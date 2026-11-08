#include "UI/NewReadablePopupMenuScreen.hpp"

#include <algorithm>
#include <cstdlib>

#include "UI/Menu/GenericSettingsMenuScreen.hpp"
#include "UI/Menu/MenuItemCheckable.hpp"
#include "UI/Menu/MenuItemSingleLine.hpp"

void NewReadablePopupMenuScreen::addSystemTab(
    const std::function<void(const std::shared_ptr<Screen> &)> &showScreen,
    const std::function<void()> &backToText) {
    addTab(
        "System",
        [this, showScreen, backToText]() {
        auto items = std::make_shared<MenuItemVector>();
        for (bool menuDepth : {true, false}) {
            items->push_back(
                std::make_shared<MenuItemSingleLine>(menuDepth ? "Menu Depth" : "Global Depth",
                                                     [this, showScreen, backToText, menuDepth]() {
                auto menu = GenericSettingsMenuScreen::stereoDepthMenu(display_, prefs_,
                                                                       "Back to Text", menuDepth);
                menu->setBackCallback(backToText);
                showScreen(menu);
                }));
        }
        items->push_back(std::make_shared<MenuItemCheckable>(
            "Alt Page Advance",
            [this]() {
            prefs_.altPageAdvance.set(prefs_.altPageAdvance.get().value_or(0) == 0 ? 1 : 0);
            setNeedsRerender();
            },
            [this]() { return prefs_.altPageAdvance.get().value_or(0) != 0; }));
        items->push_back(std::make_shared<MenuItemCheckable>(
            "Fast Transitions",
            [this]() {
            prefs_.fastTransitions.set(!prefs_.fastTransitions.get().value_or(false));
            setNeedsRerender();
            },
            [this]() { return prefs_.fastTransitions.get().value_or(false); }));
        items->push_back(std::make_shared<MenuItemSingleLine>("Back to Text", backToText));
        return items;
        },
        8);
}

NewReadablePopupMenuScreen::~NewReadablePopupMenuScreen() { releaseStereo(); }

void NewReadablePopupMenuScreen::releaseStereo() {
    if (stereoActive_) {
        display_.setStereoEnabled(false);
        stereoActive_ = false;
    }
    setFirstLoad();
}

auto NewReadablePopupMenuScreen::needsBackgroundRefresh() -> bool {
    auto oldFooter1 = footer1TextView_.getText();
    auto oldFooter2 = footer2TextView_.getText();
    auto oldFooter3 = footer3TextView_.getText();
    setUpPopupMenu();
    auto depth =
        std::clamp(prefs_.menuDisparity.get().value_or(1), 0, DisplaySystem::MAX_MENU_DISPARITY);
    if (oldFooter1 != footer1TextView_.getText() || oldFooter2 != footer2TextView_.getText() ||
        oldFooter3 != footer3TextView_.getText() || menuDepth_ != depth) {
        setFirstLoad();
    }
    return firstLoad_;
}

void NewReadablePopupMenuScreen::setUpPopupMenu() {
    std::shared_ptr<Place> place = renderer_.getCurrentPlace();

    std::string placeStringForBottom;
    std::string chapterPagesRemainingString;

    footer1TextView_.setText("");
    footer2TextView_.setText("");
    footer3TextView_.setText("");

    if (place->isFor(DocType::EPUB)) {
        auto epubFile =
            std::dynamic_pointer_cast<EPubFileRendererStream>(renderer_.getStream())->epubFile;
        auto ePubPlace = std::dynamic_pointer_cast<EPubPlace>(place);
        auto &chapterHref = ePubPlace->getHRef();
        currentChapterIdx_ = epubFile->getChapterIdx(chapterHref);

        if (currentChapterIdx_ >= 0) {
            const auto &toc = epubFile->getToc();
            auto &chapter = toc[currentChapterIdx_];
            auto spineIdx = renderer_.getStreamPlace(renderer_.getCurrentPlace()).getSpineIdx();
            // TODO(john): This isn't exactly right because `place->getOffset()` is a byte offset,
            // not a letter offset from the beginning of the chapter. The chapters always start with
            // extra html-y stuff at the start so this is always inflated by some amount.

            // If current spine item is not the first in the chapter, add offsets of previous spine
            // items in chapter to get the correct offset
            auto previousSpineOffset = epubFile->getOffsetPreviousSpineItemsInChapter(spineIdx);
            auto currentOffset =
                chapter.epubOffset + previousSpineOffset + renderer_.getCurrentPlace()->getOffset();

            // Get the end offset of the last chapter (to determine the length of the book)
            auto &lastChapter = toc[epubFile->getToc().size() - 1];
            auto lastChapterEndOffset = lastChapter.endEpubOffset;

            if (currentOffset < lastChapterEndOffset) {
                if (readable_->isArticle()) {
                    placeStringForBottom =
                        StringFormat("%d of %d page%s", SOL_PAGE_NUMBER(currentOffset),
                                     SOL_PAGE_NUMBER(lastChapterEndOffset),
                                     SOL_PAGE_NUMBER(lastChapterEndOffset) == 1 ? "" : "s");
                } else {
                    placeStringForBottom = StringFormat("%d of %d", SOL_PAGE_NUMBER(currentOffset),
                                                        SOL_PAGE_NUMBER(lastChapterEndOffset));
                }
            }

            // Determine how many Sol Pages are left in the chapter
            if (chapter.endEpubOffset > currentOffset && !readable_->isArticle()) {
                auto chapterPagesRemaining = chapter.endEpubOffset - currentOffset;
                chapterPagesRemainingString =
                    StringFormat("%d page%s left", SOL_PAGE_NUMBER(chapterPagesRemaining),
                                 SOL_PAGE_NUMBER(chapterPagesRemaining) == 1 ? "" : "s");
            }
            // log_w("Chapter %d, %s, current:%d=%" PRIu32 "+%" PRIu32 "(%" PRIu32 ") end: %"
            // PRIu32,
            //       currentChapterIdx_, chapter.title.c_str(), currentOffset, chapter.epubOffset,
            //       renderer_.getTxtOffset(), renderer_.getCurrentPlace()->getOffset(),
            //       lastChapterEndOffset);
        } else {
            log_w("Couldn't find chapter index (%d) for href %s", currentChapterIdx_,
                  chapterHref.c_str());
        }
    } else {
        // Use explicit page numbers if provided
        if (hasExplicitPageNumbers()) {
            placeStringForBottom =
                StringFormat("%d of %d", explicitCurrentPage_, explicitTotalPages_);
        } else if (!suppressFallbackPageNumbers_) {
            // Calculate page numbers from offset if not suppressed
            size_t lengthOfFile = renderer_.size();
            placeStringForBottom =
                StringFormat("%d of %d", SOL_PAGE_NUMBER(renderer_.getTxtOffset()),
                             SOL_PAGE_NUMBER(lengthOfFile));
        }
        // If fallback is suppressed and no explicit pages, don't show anything
    }

    bool showFooter = getCurrentTabTitle() != "Style" && getCurrentTabTitle() != "System";
    if (showFooter && showPageIndicators_) {
        if (!chapterPagesRemainingString.empty()) {
            footer1TextView_.setText(chapterPagesRemainingString);
        }
        if (!placeStringForBottom.empty()) {
            footer2TextView_.setText(placeStringForBottom);
        }
    }

    SolClock &theClock = SolClock::getInstance();
    if (showFooter && theClock.isValidTime()) {
        auto timeString = theClock.getTimeString();
        footer3TextView_.setText(timeString);
    }

    setNeedsLayout();
}

void NewReadablePopupMenuScreen::preselectIndexAt(int index) { menuView_.setSelectedIndex(index); }

void NewReadablePopupMenuScreen::drawInRect(SolRect rect) {
    popupMenuBackground_.fillColor = display_.bg;
    popupMenuBackground_.strokeColor = display_.bg;
    popupMenuForeground_.strokeColor = display_.fg;

    popupMenuBackground_.draw();
    popupMenuForeground_.draw();

    titleView_.draw();
    menuView_.draw();

    footer1TextView_.draw();
    footer2TextView_.draw();
    footer3TextView_.draw();
}

void NewReadablePopupMenuScreen::drawForEye(ScreenSide eye, bool fastOnly) {
    int16_t cardShift = DisplaySystem::eyeShift(eye, menuDepth_);

    auto baseFrame = frame();
    setFrame(baseFrame.offset({cardShift, 0}));
    if (fastOnly) {
        menuView_.drawFastElements();
    } else {
        draw();
    }

    setFrame(baseFrame);
}

auto NewReadablePopupMenuScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    auto contentFrame = context.setSublayoutFrame(
        SolRect::fromSize(size), LayoutOptionsSizeToFit | LayoutOptionsAlignCenter,
        [this](LayoutContext &context, SolSize size) -> SolSize {
            int y = 0;
            auto titleBox = context.setViewFrame(titleView_, {{0, y}, size},
                                                 LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                                                     LayoutOptionsAlignCenterHorizontal);

            y += titleBox.size.height + 12;

            auto menuBox = context.setViewFrame(
                menuView_, {{10, y}, {size.width - 10, size.height - y}},
                LayoutOptionsSizeToFit | LayoutOptionsAlignTop | LayoutOptionsAlignLeft);

            y += menuBox.size.height;

            // If there is at least one footer, advance the y
            if (!footer1TextView_.getText().empty() || !footer2TextView_.getText().empty() ||
                !footer3TextView_.getText().empty()) {
                y += 10;
            }

            if (!footer1TextView_.getText().empty()) {
                auto footer1Rect = context.setViewFrame(footer1TextView_, {{0, y}, size},
                                                        LayoutOptionsSizeToFit |
                                                            LayoutOptionsAlignCenterHorizontal);
                y += footer1Rect.size.height;
            }
            if (!footer2TextView_.getText().empty()) {
                auto footer2Rect = context.setViewFrame(footer2TextView_, {{0, y}, size},
                                                        LayoutOptionsSizeToFit |
                                                            LayoutOptionsAlignCenterHorizontal);
                y += footer2Rect.size.height;
            }
            if (!footer3TextView_.getText().empty()) {
                auto footer3Rect = context.setViewFrame(footer3TextView_, {{0, y}, size},
                                                        LayoutOptionsSizeToFit |
                                                            LayoutOptionsAlignCenterHorizontal);
                y += footer3Rect.size.height;
            }

            // Keep widths stable within each tab, with more room for System setting labels.
            int width = getCurrentTabTitle() == "System" ? size.width * 3 / 4 - 18
                                                         : size.width * 5 / 9 - 18;
            return {width, y + 1};
        });

    auto foregroundFrame = context.setViewFrame(popupMenuForeground_, contentFrame.inset(-5, -10));

    auto backgroundFrame = context.setViewFrame(popupMenuBackground_, foregroundFrame.inset(-5));

    return backgroundFrame.size;
}

void NewReadablePopupMenuScreen::render(DisplaySystem &display) {
    bool refreshBackground = needsBackgroundRefresh();
    menuDepth_ =
        std::clamp(prefs_.menuDisparity.get().value_or(1), 0, DisplaySystem::MAX_MENU_DISPARITY);
    stereoActive_ = menuDepth_ != 0 && display.setStereoEnabled(true);

    setFrame(defaultFrameRect());
    layoutIfNeeded();

    display.drawForEachEye([this](ScreenSide eye) { drawForEye(eye); });

    if (refreshBackground) {
        firstLoad_ = false;
        display.updateDisplay(menuDepth_ != 0 ? DisplaySystem::UpdateType::FULL
                                              : DisplaySystem::UpdateType::PARTIAL,
                              menuDepth_ == 0);
        // Redraw but this time without the text (since updateDisplay will clear the screen)
        if (menuDepth_ == 0) {
            display.drawForEachEye([this](ScreenSide eye) { drawForEye(eye); });
        }
    }

    display.drawForEachEye([this](ScreenSide eye) { drawForEye(eye, true); });
    auto updateRect = SolRect::zero();
    if (!refreshBackground || menuDepth_ != 0) {
        updateRect = popupMenuBackground_.screenFrame();
        auto leftShift = DisplaySystem::eyeShift(ScreenSide::Left, menuDepth_);
        auto rightShift = DisplaySystem::eyeShift(ScreenSide::Right, menuDepth_);
        updateRect.origin.x += std::min(leftShift, rightShift);
        updateRect.size.width += std::abs(leftShift - rightShift);
    }
    display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR, false, 0,
                          DisplaySystem::UpdateType::UNSPECIFIED, ScreenSide::Both, updateRect);
}
