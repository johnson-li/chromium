// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/test_protocol_handler.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace headless {
namespace {
const char kMainPageCookie[] = "mood=quizzical";
const char kIsolatedPageCookie[] = "mood=quixotic";
}  // namespace

// This test creates two tabs pointing to the same security origin in two
// different browser contexts and checks that they are isolated by creating two
// cookies with the same name in both tabs. The steps are:
//
// 1. Wait for tab #1 to become ready for DevTools.
// 2. Create tab #2 and wait for it to become ready for DevTools.
// 3. Navigate tab #1 to the test page and wait for it to finish loading.
// 4. Navigate tab #2 to the test page and wait for it to finish loading.
// 5. Set a cookie in tab #1.
// 6. Set the same cookie in tab #2 to a different value.
// 7. Read the cookie in tab #1 and check that it has the first value.
// 8. Read the cookie in tab #2 and check that it has the second value.
//
// If the tabs aren't properly isolated, step 7 will fail.
class HeadlessBrowserContextIsolationTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  HeadlessBrowserContextIsolationTest()
      : browser_context_(nullptr),
        web_contents2_(nullptr),
        devtools_client2_(HeadlessDevToolsClient::Create()) {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override {
    if (!web_contents2_) {
      browser_context_ = browser()->CreateBrowserContextBuilder().Build();
      web_contents2_ = browser_context_->CreateWebContentsBuilder().Build();
      web_contents2_->AddObserver(this);
      return;
    }

    web_contents2_->GetDevToolsTarget()->AttachClient(devtools_client2_.get());
    HeadlessAsyncDevTooledBrowserTest::DevToolsTargetReady();
  }

  void RunDevTooledTest() override {
    load_observer_.reset(new LoadObserver(
        devtools_client_.get(),
        base::Bind(&HeadlessBrowserContextIsolationTest::OnFirstLoadComplete,
                   base::Unretained(this))));
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnFirstLoadComplete() {
    EXPECT_TRUE(load_observer_->navigation_succeeded());
    load_observer_.reset(new LoadObserver(
        devtools_client2_.get(),
        base::Bind(&HeadlessBrowserContextIsolationTest::OnSecondLoadComplete,
                   base::Unretained(this))));
    devtools_client2_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnSecondLoadComplete() {
    EXPECT_TRUE(load_observer_->navigation_succeeded());
    load_observer_.reset();

    devtools_client_->GetRuntime()->Evaluate(
        base::StringPrintf("document.cookie = '%s'", kMainPageCookie),
        base::Bind(&HeadlessBrowserContextIsolationTest::OnFirstSetCookieResult,
                   base::Unretained(this)));
  }

  void OnFirstSetCookieResult(std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kMainPageCookie, cookie);

    devtools_client2_->GetRuntime()->Evaluate(
        base::StringPrintf("document.cookie = '%s'", kIsolatedPageCookie),
        base::Bind(
            &HeadlessBrowserContextIsolationTest::OnSecondSetCookieResult,
            base::Unretained(this)));
  }

  void OnSecondSetCookieResult(
      std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kIsolatedPageCookie, cookie);

    devtools_client_->GetRuntime()->Evaluate(
        "document.cookie",
        base::Bind(&HeadlessBrowserContextIsolationTest::OnFirstGetCookieResult,
                   base::Unretained(this)));
  }

  void OnFirstGetCookieResult(std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kMainPageCookie, cookie);

    devtools_client2_->GetRuntime()->Evaluate(
        "document.cookie",
        base::Bind(
            &HeadlessBrowserContextIsolationTest::OnSecondGetCookieResult,
            base::Unretained(this)));
  }

  void OnSecondGetCookieResult(
      std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kIsolatedPageCookie, cookie);
    FinishTest();
  }

  void FinishTest() {
    web_contents2_->RemoveObserver(this);
    web_contents2_->Close();
    browser_context_->Close();
    FinishAsynchronousTest();
  }

 private:
  HeadlessBrowserContext* browser_context_;
  HeadlessWebContents* web_contents2_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client2_;
  std::unique_ptr<LoadObserver> load_observer_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessBrowserContextIsolationTest);

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, ContextProtocolHandler) {
  const std::string kResponseBody = "<p>HTTP response body</p>";
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpScheme] =
      base::MakeUnique<TestProtocolHandler>(kResponseBody);

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProtocolHandlers(std::move(protocol_handlers))
          .Build();
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
  web_contents->Close();

  HeadlessBrowserContext* another_browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  // Loading the same non-existent page using a tab with a different context
  // should not work since the protocol handler only exists on the custom
  // context.
  web_contents =
      another_browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_FALSE(WaitForLoad(web_contents));
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ("", inner_html);
  web_contents->Close();
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, UserDataDir) {
  // We do not want to bother with posting tasks to create a temp dir.
  // Just allow IO from main thread for now.
  base::ThreadRestrictions::SetIOAllowed(true);

  EXPECT_TRUE(embedded_test_server()->Start());

  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());

  // Newly created temp directory should be empty.
  EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir.GetPath()));

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetUserDataDir(user_data_dir.GetPath())
          .SetIncognitoMode(false)
          .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();

  EXPECT_TRUE(WaitForLoad(web_contents));

  // Something should be written to this directory.
  // If it is not the case, more complex page may be needed.
  // ServiceWorkers may be a good option.
  EXPECT_FALSE(base::IsDirectoryEmpty(user_data_dir.GetPath()));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, IncognitoMode) {
  // We do not want to bother with posting tasks to create a temp dir.
  // Just allow IO from main thread for now.
  base::ThreadRestrictions::SetIOAllowed(true);

  EXPECT_TRUE(embedded_test_server()->Start());

  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());

  // Newly created temp directory should be empty.
  EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir.GetPath()));

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetUserDataDir(user_data_dir.GetPath())
          .SetIncognitoMode(true)
          .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();

  EXPECT_TRUE(WaitForLoad(web_contents));

  // Similar to test above, but now we are in incognito mode,
  // so nothing should be written to this directory.
  EXPECT_TRUE(base::IsDirectoryEmpty(user_data_dir.GetPath()));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, ContextWebPreferences) {
  // By default, hide_scrollbars should be false.
  EXPECT_FALSE(WebPreferences().hide_scrollbars);

  // Set hide_scrollbars preference to true for a new BrowserContext.
  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetOverrideWebPreferencesCallback(
              base::Bind([](WebPreferences* preferences) {
                preferences->hide_scrollbars = true;
              }))
          .Build();
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("about:blank"))
          .Build();

  // Verify that the preference takes effect.
  HeadlessWebContentsImpl* contents_impl =
      HeadlessWebContentsImpl::From(web_contents);
  EXPECT_TRUE(contents_impl->web_contents()
                  ->GetRenderViewHost()
                  ->GetWebkitPreferences().hide_scrollbars);
}

}  // namespace headless
