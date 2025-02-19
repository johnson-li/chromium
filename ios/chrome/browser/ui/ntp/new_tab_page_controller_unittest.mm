// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/ntp/modal_ntp.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/chrome/test/testing_application_context.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


namespace {

class NewTabPageControllerTest : public BlockCleanupTest {
 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();

    // Set up a test ChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();

    // Load TemplateURLService.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    chrome_browser_state_->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    GURL url(kChromeUINewTabURL);
    parentViewController_ = [[UIViewController alloc] init];
    tabModel_ = [[TabModel alloc]
        initWithSessionWindow:nil
               sessionService:[[TestSessionService alloc] init]
                 browserState:chrome_browser_state_.get()];
    controller_ =
        [[NewTabPageController alloc] initWithUrl:url
                                           loader:nil
                                          focuser:nil
                                      ntpObserver:nil
                                     browserState:chrome_browser_state_.get()
                                       colorCache:nil
                                  toolbarDelegate:nil
                                         tabModel:tabModel_
                             parentViewController:parentViewController_
                                       dispatcher:nil
                                    safeAreaInset:UIEdgeInsetsZero];

    incognitoController_ = [[NewTabPageController alloc]
                 initWithUrl:url
                      loader:nil
                     focuser:nil
                 ntpObserver:nil
                browserState:chrome_browser_state_
                                 ->GetOffTheRecordChromeBrowserState()
                  colorCache:nil
             toolbarDelegate:nil
                    tabModel:nil
        parentViewController:parentViewController_
                  dispatcher:nil
               safeAreaInset:UIEdgeInsetsZero];
  };

  void TearDown() override {
    incognitoController_ = nil;
    controller_ = nil;
    parentViewController_ = nil;
    [tabModel_ browserStateDestroyed];
    tabModel_ = nil;

    // There may be blocks released below that have weak references to |profile|
    // owned by chrome_browser_state_.  Ensure BlockCleanupTest::TearDown() is
    // called before |chrome_browser_state_| is reset.
    BlockCleanupTest::TearDown();
    chrome_browser_state_.reset();
  }

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TabModel* tabModel_;
  UIViewController* parentViewController_;
  NewTabPageController* controller_;
  NewTabPageController* incognitoController_;
};

TEST_F(NewTabPageControllerTest, NewTabBarItemDidChange) {
  // Switching the selected index in the NewTabPageBar should cause
  // newTabBarItemDidChange to get called.
  NewTabPageView* NTPView =
      base::mac::ObjCCastStrict<NewTabPageView>(controller_.view);
  NewTabPageBar* bar = [NTPView tabBar];
  NSUInteger bookmarkIndex = 0;
  UIButton* button = [[bar buttons] objectAtIndex:bookmarkIndex];
  UIControlEvents event = !PresentNTPPanelModally()
                              ? UIControlEventTouchDown
                              : UIControlEventTouchUpInside;
  [button sendActionsForControlEvents:event];

  // Expecting bookmarks panel to be loaded now and to be the current controller
  // on iPad but not iPhone.
  // Deliberately comparing pointers.
  if (!PresentNTPPanelModally()) {
    EXPECT_EQ([controller_ currentController],
              (id<NewTabPagePanelProtocol>)[controller_ bookmarkController]);
  } else {
    EXPECT_NE([controller_ currentController],
              (id<NewTabPagePanelProtocol>)[controller_ bookmarkController]);
  }
}

TEST_F(NewTabPageControllerTest, SelectBookmarkPanel) {
  // Expecting on start up that the bookmarkController does not exist.
  // Deliberately comparing pointers.
  EXPECT_NE([controller_ currentController],
            (id<NewTabPagePanelProtocol>)[controller_ bookmarkController]);

  // Switching to the Bookmarks panel.
  [controller_ selectPanel:ntp_home::BOOKMARKS_PANEL];

  // Expecting bookmarks panel to be loaded now and to be the current controller
  // on iPad but not iPhone.
  // Deliberately comparing pointers.
  if (!PresentNTPPanelModally()) {
    EXPECT_EQ([controller_ currentController],
              (id<NewTabPagePanelProtocol>)[controller_ bookmarkController]);
  } else {
    EXPECT_NE([controller_ currentController],
              (id<NewTabPagePanelProtocol>)[controller_ bookmarkController]);
  }
}

TEST_F(NewTabPageControllerTest, SelectIncognitoPanel) {
  // Expect on start up that the Incognito panel is the default.
  EXPECT_EQ(
      (id<NewTabPagePanelProtocol>)[incognitoController_ incognitoController],
      [incognitoController_ currentController]);

  // Switch to the Bookmarks panel.
  [incognitoController_ selectPanel:ntp_home::BOOKMARKS_PANEL];

  // Expecting bookmarks panel to be loaded now and to be the current controller
  // on iPad but not iPhone.
  // Deliberately comparing pointers.
  if (!PresentNTPPanelModally()) {
    EXPECT_EQ(
        [incognitoController_ currentController],
        (id<NewTabPagePanelProtocol>)[incognitoController_ bookmarkController]);
  } else {
    EXPECT_NE(
        [incognitoController_ currentController],
        (id<NewTabPagePanelProtocol>)[incognitoController_ bookmarkController]);
  }
}

TEST_F(NewTabPageControllerTest, TestWantsLocationBarHintText) {
  // Default NTP doesn't show location bar hint text on iPad, and it does on
  // iPhone.
  if (IsIPadIdiom())
    EXPECT_EQ(NO, [controller_ wantsLocationBarHintText]);
  else
    EXPECT_EQ(YES, [controller_ wantsLocationBarHintText]);

  // Default incognito always does.
  EXPECT_EQ(YES, [incognitoController_ wantsLocationBarHintText]);
}

TEST_F(NewTabPageControllerTest, NewTabPageIdentifierConversion) {
  EXPECT_EQ("open_tabs",
            NewTabPage::FragmentFromIdentifier(ntp_home::RECENT_TABS_PANEL));
  EXPECT_EQ("", NewTabPage::FragmentFromIdentifier(ntp_home::NONE));
  EXPECT_EQ(ntp_home::BOOKMARKS_PANEL,
            NewTabPage::IdentifierFromFragment("bookmarks"));
  EXPECT_EQ(ntp_home::NONE, NewTabPage::IdentifierFromFragment("garbage"));
  EXPECT_EQ(ntp_home::NONE, NewTabPage::IdentifierFromFragment(""));
}

}  // anonymous namespace
