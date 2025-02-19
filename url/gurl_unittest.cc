// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_constants.h"
#include "url/url_test_utils.h"

namespace url {

namespace {

template<typename CHAR>
void SetupReplacement(
    void (Replacements<CHAR>::*func)(const CHAR*, const Component&),
    Replacements<CHAR>* replacements,
    const CHAR* str) {
  if (str) {
    Component comp;
    if (str[0])
      comp.len = static_cast<int>(strlen(str));
    (replacements->*func)(str, comp);
  }
}

// Returns the canonicalized string for the given URL string for the
// GURLTest.Types test.
std::string TypesTestCase(const char* src) {
  GURL gurl(src);
  return gurl.possibly_invalid_spec();
}

}  // namespace

// Different types of URLs should be handled differently, and handed off to
// different canonicalizers.
TEST(GURLTest, Types) {
  // URLs with unknown schemes should be treated as path URLs, even when they
  // have things like "://".
  EXPECT_EQ("something:///HOSTNAME.com/",
            TypesTestCase("something:///HOSTNAME.com/"));

  // Conversely, URLs with known schemes should always trigger standard URL
  // handling.
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http:HOSTNAME.com"));
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http:/HOSTNAME.com"));
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http://HOSTNAME.com"));
  EXPECT_EQ("http://hostname.com/", TypesTestCase("http:///HOSTNAME.com"));

#ifdef WIN32
  // URLs that look like Windows absolute path specs.
  EXPECT_EQ("file:///C:/foo.txt", TypesTestCase("c:\\foo.txt"));
  EXPECT_EQ("file:///Z:/foo.txt", TypesTestCase("Z|foo.txt"));
  EXPECT_EQ("file://server/foo.txt", TypesTestCase("\\\\server\\foo.txt"));
  EXPECT_EQ("file://server/foo.txt", TypesTestCase("//server/foo.txt"));
#endif
}

// Test the basic creation and querying of components in a GURL. We assume that
// the parser is already tested and works, so we are mostly interested if the
// object does the right thing with the results.
TEST(GURLTest, Components) {
  GURL empty_url(base::UTF8ToUTF16(""));
  EXPECT_TRUE(empty_url.is_empty());
  EXPECT_FALSE(empty_url.is_valid());

  GURL url(base::UTF8ToUTF16("http://user:pass@google.com:99/foo;bar?q=a#ref"));
  EXPECT_FALSE(url.is_empty());
  EXPECT_TRUE(url.is_valid());
  EXPECT_TRUE(url.SchemeIs("http"));
  EXPECT_FALSE(url.SchemeIsFile());

  // This is the narrow version of the URL, which should match the wide input.
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url.spec());

  EXPECT_EQ("http", url.scheme());
  EXPECT_EQ("user", url.username());
  EXPECT_EQ("pass", url.password());
  EXPECT_EQ("google.com", url.host());
  EXPECT_EQ("99", url.port());
  EXPECT_EQ(99, url.IntPort());
  EXPECT_EQ("/foo;bar", url.path());
  EXPECT_EQ("q=a", url.query());
  EXPECT_EQ("ref", url.ref());

  // Test parsing userinfo with special characters.
  GURL url_special_pass("http://user:%40!$&'()*+,;=:@google.com:12345");
  EXPECT_TRUE(url_special_pass.is_valid());
  // GURL canonicalizes some delimiters.
  EXPECT_EQ("%40!$&%27()*+,%3B%3D%3A", url_special_pass.password());
  EXPECT_EQ("google.com", url_special_pass.host());
  EXPECT_EQ("12345", url_special_pass.port());
}

TEST(GURLTest, Empty) {
  GURL url;
  EXPECT_FALSE(url.is_valid());
  EXPECT_EQ("", url.spec());

  EXPECT_EQ("", url.scheme());
  EXPECT_EQ("", url.username());
  EXPECT_EQ("", url.password());
  EXPECT_EQ("", url.host());
  EXPECT_EQ("", url.port());
  EXPECT_EQ(PORT_UNSPECIFIED, url.IntPort());
  EXPECT_EQ("", url.path());
  EXPECT_EQ("", url.query());
  EXPECT_EQ("", url.ref());
}

TEST(GURLTest, Copy) {
  GURL url(base::UTF8ToUTF16(
      "http://user:pass@google.com:99/foo;bar?q=a#ref"));

  GURL url2(url);
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("http", url2.scheme());
  EXPECT_EQ("user", url2.username());
  EXPECT_EQ("pass", url2.password());
  EXPECT_EQ("google.com", url2.host());
  EXPECT_EQ("99", url2.port());
  EXPECT_EQ(99, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  // Copying of invalid URL should be invalid
  GURL invalid;
  GURL invalid2(invalid);
  EXPECT_FALSE(invalid2.is_valid());
  EXPECT_EQ("", invalid2.spec());
  EXPECT_EQ("", invalid2.scheme());
  EXPECT_EQ("", invalid2.username());
  EXPECT_EQ("", invalid2.password());
  EXPECT_EQ("", invalid2.host());
  EXPECT_EQ("", invalid2.port());
  EXPECT_EQ(PORT_UNSPECIFIED, invalid2.IntPort());
  EXPECT_EQ("", invalid2.path());
  EXPECT_EQ("", invalid2.query());
  EXPECT_EQ("", invalid2.ref());
}

TEST(GURLTest, Assign) {
  GURL url(base::UTF8ToUTF16(
      "http://user:pass@google.com:99/foo;bar?q=a#ref"));

  GURL url2;
  url2 = url;
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("http", url2.scheme());
  EXPECT_EQ("user", url2.username());
  EXPECT_EQ("pass", url2.password());
  EXPECT_EQ("google.com", url2.host());
  EXPECT_EQ("99", url2.port());
  EXPECT_EQ(99, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  // Assignment of invalid URL should be invalid
  GURL invalid;
  GURL invalid2;
  invalid2 = invalid;
  EXPECT_FALSE(invalid2.is_valid());
  EXPECT_EQ("", invalid2.spec());
  EXPECT_EQ("", invalid2.scheme());
  EXPECT_EQ("", invalid2.username());
  EXPECT_EQ("", invalid2.password());
  EXPECT_EQ("", invalid2.host());
  EXPECT_EQ("", invalid2.port());
  EXPECT_EQ(PORT_UNSPECIFIED, invalid2.IntPort());
  EXPECT_EQ("", invalid2.path());
  EXPECT_EQ("", invalid2.query());
  EXPECT_EQ("", invalid2.ref());
}

// This is a regression test for http://crbug.com/309975.
TEST(GURLTest, SelfAssign) {
  GURL a("filesystem:http://example.com/temporary/");
  // This should not crash.
  a = a;
}

TEST(GURLTest, CopyFileSystem) {
  GURL url(base::UTF8ToUTF16(
      "filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref"));

  GURL url2(url);
  EXPECT_TRUE(url2.is_valid());

  EXPECT_EQ("filesystem:https://user:pass@google.com:99/t/foo;bar?q=a#ref", url2.spec());
  EXPECT_EQ("filesystem", url2.scheme());
  EXPECT_EQ("", url2.username());
  EXPECT_EQ("", url2.password());
  EXPECT_EQ("", url2.host());
  EXPECT_EQ("", url2.port());
  EXPECT_EQ(PORT_UNSPECIFIED, url2.IntPort());
  EXPECT_EQ("/foo;bar", url2.path());
  EXPECT_EQ("q=a", url2.query());
  EXPECT_EQ("ref", url2.ref());

  const GURL* inner = url2.inner_url();
  ASSERT_TRUE(inner);
  EXPECT_EQ("https", inner->scheme());
  EXPECT_EQ("user", inner->username());
  EXPECT_EQ("pass", inner->password());
  EXPECT_EQ("google.com", inner->host());
  EXPECT_EQ("99", inner->port());
  EXPECT_EQ(99, inner->IntPort());
  EXPECT_EQ("/t", inner->path());
  EXPECT_EQ("", inner->query());
  EXPECT_EQ("", inner->ref());
}

TEST(GURLTest, IsValid) {
  const char* valid_cases[] = {
    "http://google.com",
    "unknown://google.com",
    "http://user:pass@google.com",
    "http://google.com:12345",
    "http://google.com/path",
    "http://google.com//path",
    "http://google.com?k=v#fragment",
    "http://user:pass@google.com:12345/path?k=v#fragment",
    "http:/path",
    "http:path",
  };
  for (size_t i = 0; i < arraysize(valid_cases); i++) {
    EXPECT_TRUE(GURL(valid_cases[i]).is_valid())
        << "Case: " << valid_cases[i];
  }

  const char* invalid_cases[] = {
    "http://?k=v",
    "http:://google.com",
    "http//google.com",
    "http://google.com:12three45",
    "://google.com",
    "path",
  };
  for (size_t i = 0; i < arraysize(invalid_cases); i++) {
    EXPECT_FALSE(GURL(invalid_cases[i]).is_valid())
        << "Case: " << invalid_cases[i];
  }
}

TEST(GURLTest, ExtraSlashesBeforeAuthority) {
  // According to RFC3986, the hierarchical part for URI with an authority
  // must use only two slashes; GURL intentionally just ignores extra slashes
  // if there are more than 2, and parses the following part as an authority.
  GURL url("http:///host");
  EXPECT_EQ("host", url.host());
  EXPECT_EQ("/", url.path());
}

// Given an invalid URL, we should still get most of the components.
TEST(GURLTest, ComponentGettersWorkEvenForInvalidURL) {
  GURL url("http:google.com:foo");
  EXPECT_FALSE(url.is_valid());
  EXPECT_EQ("http://google.com:foo/", url.possibly_invalid_spec());

  EXPECT_EQ("http", url.scheme());
  EXPECT_EQ("", url.username());
  EXPECT_EQ("", url.password());
  EXPECT_EQ("google.com", url.host());
  EXPECT_EQ("foo", url.port());
  EXPECT_EQ(PORT_INVALID, url.IntPort());
  EXPECT_EQ("/", url.path());
  EXPECT_EQ("", url.query());
  EXPECT_EQ("", url.ref());
}

TEST(GURLTest, Resolve) {
  // The tricky cases for relative URL resolving are tested in the
  // canonicalizer unit test. Here, we just test that the GURL integration
  // works properly.
  struct ResolveCase {
    const char* base;
    const char* relative;
    bool expected_valid;
    const char* expected;
  } resolve_cases[] = {
    {"http://www.google.com/", "foo.html", true, "http://www.google.com/foo.html"},
    {"http://www.google.com/foo/", "bar", true, "http://www.google.com/foo/bar"},
    {"http://www.google.com/foo/", "/bar", true, "http://www.google.com/bar"},
    {"http://www.google.com/foo", "bar", true, "http://www.google.com/bar"},
    {"http://www.google.com/", "http://images.google.com/foo.html", true, "http://images.google.com/foo.html"},
    {"http://www.google.com/", "http://images.\tgoogle.\ncom/\rfoo.html", true, "http://images.google.com/foo.html"},
    {"http://www.google.com/blah/bloo?c#d", "../../../hello/./world.html?a#b", true, "http://www.google.com/hello/world.html?a#b"},
    {"http://www.google.com/foo#bar", "#com", true, "http://www.google.com/foo#com"},
    {"http://www.google.com/", "Https:images.google.com", true, "https://images.google.com/"},
      // A non-standard base can be replaced with a standard absolute URL.
    {"data:blahblah", "http://google.com/", true, "http://google.com/"},
    {"data:blahblah", "http:google.com", true, "http://google.com/"},
      // Filesystem URLs have different paths to test.
    {"filesystem:http://www.google.com/type/", "foo.html", true, "filesystem:http://www.google.com/type/foo.html"},
    {"filesystem:http://www.google.com/type/", "../foo.html", true, "filesystem:http://www.google.com/type/foo.html"},
  };

  for (size_t i = 0; i < arraysize(resolve_cases); i++) {
    // 8-bit code path.
    GURL input(resolve_cases[i].base);
    GURL output = input.Resolve(resolve_cases[i].relative);
    EXPECT_EQ(resolve_cases[i].expected_valid, output.is_valid()) << i;
    EXPECT_EQ(resolve_cases[i].expected, output.spec()) << i;
    EXPECT_EQ(output.SchemeIsFileSystem(), output.inner_url() != NULL);

    // Wide code path.
    GURL inputw(base::UTF8ToUTF16(resolve_cases[i].base));
    GURL outputw =
        input.Resolve(base::UTF8ToUTF16(resolve_cases[i].relative));
    EXPECT_EQ(resolve_cases[i].expected_valid, outputw.is_valid()) << i;
    EXPECT_EQ(resolve_cases[i].expected, outputw.spec()) << i;
    EXPECT_EQ(outputw.SchemeIsFileSystem(), outputw.inner_url() != NULL);
  }
}

TEST(GURLTest, GetOrigin) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "http://www.google.com/"},
    {"javascript:window.alert(\"hello,world\");", ""},
    {"http://user:pass@www.google.com:21/blah#baz", "http://www.google.com:21/"},
    {"http://user@www.google.com", "http://www.google.com/"},
    {"http://:pass@www.google.com", "http://www.google.com/"},
    {"http://:@www.google.com", "http://www.google.com/"},
    {"filesystem:http://www.google.com/temp/foo?q#b", "http://www.google.com/"},
    {"filesystem:http://user:pass@google.com:21/blah#baz", "http://google.com:21/"},
  };
  for (size_t i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    GURL origin = url.GetOrigin();
    EXPECT_EQ(cases[i].expected, origin.spec());
  }
}

TEST(GURLTest, GetAsReferrer) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "http://www.google.com/"},
    {"http://user:pass@www.google.com:21/blah#baz", "http://www.google.com:21/blah"},
    {"http://user@www.google.com", "http://www.google.com/"},
    {"http://:pass@www.google.com", "http://www.google.com/"},
    {"http://:@www.google.com", "http://www.google.com/"},
    {"http://www.google.com/temp/foo?q#b", "http://www.google.com/temp/foo?q"},
    {"not a url", ""},
    {"unknown-scheme://foo.html", ""},
    {"file:///tmp/test.html", ""},
    {"https://www.google.com", "https://www.google.com/"},
  };
  for (size_t i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    GURL origin = url.GetAsReferrer();
    EXPECT_EQ(cases[i].expected, origin.spec());
  }
}

TEST(GURLTest, GetWithEmptyPath) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    {"http://www.google.com", "http://www.google.com/"},
    {"javascript:window.alert(\"hello, world\");", ""},
    {"http://www.google.com/foo/bar.html?baz=22", "http://www.google.com/"},
    {"filesystem:http://www.google.com/temporary/bar.html?baz=22", "filesystem:http://www.google.com/temporary/"},
    {"filesystem:file:///temporary/bar.html?baz=22", "filesystem:file:///temporary/"},
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    GURL empty_path = url.GetWithEmptyPath();
    EXPECT_EQ(cases[i].expected, empty_path.spec());
  }
}

TEST(GURLTest, GetWithoutFilename) {
  struct TestCase {
    const char* input;
    const char* expected;
  } cases[] = {
    // Common Standard URLs.
    {"https://www.google.com",                    "https://www.google.com/"},
    {"https://www.google.com/",                   "https://www.google.com/"},
    {"https://www.google.com/maps.htm",           "https://www.google.com/"},
    {"https://www.google.com/maps/",              "https://www.google.com/maps/"},
    {"https://www.google.com/index.html",         "https://www.google.com/"},
    {"https://www.google.com/index.html?q=maps",  "https://www.google.com/"},
    {"https://www.google.com/index.html#maps/",   "https://www.google.com/"},
    {"https://foo:bar@www.google.com/maps.htm",   "https://foo:bar@www.google.com/"},
    {"https://www.google.com/maps/au/index.html", "https://www.google.com/maps/au/"},
    {"https://www.google.com/maps/au/north",      "https://www.google.com/maps/au/"},
    {"https://www.google.com/maps/au/north/",     "https://www.google.com/maps/au/north/"},
    {"https://www.google.com/maps/au/index.html?q=maps#fragment/",     "https://www.google.com/maps/au/"},
    {"http://www.google.com:8000/maps/au/index.html?q=maps#fragment/", "http://www.google.com:8000/maps/au/"},
    {"https://www.google.com/maps/au/north/?q=maps#fragment",          "https://www.google.com/maps/au/north/"},
    {"https://www.google.com/maps/au/north?q=maps#fragment",           "https://www.google.com/maps/au/"},
    // Less common standard URLs.
    {"filesystem:http://www.google.com/temporary/bar.html?baz=22", "filesystem:http://www.google.com/temporary/"},
    {"file:///temporary/bar.html?baz=22","file:///temporary/"},
    {"ftp://foo/test/index.html",        "ftp://foo/test/"},
    {"gopher://foo/test/index.html",     "gopher://foo/test/"},
    {"ws://foo/test/index.html",         "ws://foo/test/"},
    // Non-standard, hierarchical URLs.
    {"chrome://foo/bar.html", "chrome://foo/"},
    {"httpa://foo/test/index.html", "httpa://foo/test/"},
    // Non-standard, non-hierarchical URLs.
    {"blob:https://foo.bar/test/index.html", ""},
    {"about:blank", ""},
    {"data:foobar", ""},
    {"scheme:opaque_data", ""},
    // Invalid URLs.
    {"foobar", ""},
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    GURL without_filename = url.GetWithoutFilename();
    EXPECT_EQ(cases[i].expected, without_filename.spec()) << i;
  }
}

TEST(GURLTest, Replacements) {
  // The URL canonicalizer replacement test will handle most of these case.
  // The most important thing to do here is to check that the proper
  // canonicalizer gets called based on the scheme of the input.
  struct ReplaceCase {
    const char* base;
    const char* scheme;
    const char* username;
    const char* password;
    const char* host;
    const char* port;
    const char* path;
    const char* query;
    const char* ref;
    const char* expected;
  } replace_cases[] = {
      {"http://www.google.com/foo/bar.html?foo#bar", NULL, NULL, NULL, NULL,
       NULL, "/", "", "", "http://www.google.com/"},
      {"http://www.google.com/foo/bar.html?foo#bar", "javascript", "", "", "",
       "", "window.open('foo');", "", "", "javascript:window.open('foo');"},
      {"file:///C:/foo/bar.txt", "http", NULL, NULL, "www.google.com", "99",
       "/foo", "search", "ref", "http://www.google.com:99/foo?search#ref"},
#ifdef WIN32
      {"http://www.google.com/foo/bar.html?foo#bar", "file", "", "", "", "",
       "c:\\", "", "", "file:///C:/"},
#endif
      {"filesystem:http://www.google.com/foo/bar.html?foo#bar", NULL, NULL,
       NULL, NULL, NULL, "/", "", "", "filesystem:http://www.google.com/foo/"},
      // Lengthen the URL instead of shortening it, to test creation of
      // inner_url.
      {"filesystem:http://www.google.com/foo/", NULL, NULL, NULL, NULL, NULL,
       "bar.html", "foo", "bar",
       "filesystem:http://www.google.com/foo/bar.html?foo#bar"},
  };

  for (size_t i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    GURL url(cur.base);
    GURL::Replacements repl;
    SetupReplacement(&GURL::Replacements::SetScheme, &repl, cur.scheme);
    SetupReplacement(&GURL::Replacements::SetUsername, &repl, cur.username);
    SetupReplacement(&GURL::Replacements::SetPassword, &repl, cur.password);
    SetupReplacement(&GURL::Replacements::SetHost, &repl, cur.host);
    SetupReplacement(&GURL::Replacements::SetPort, &repl, cur.port);
    SetupReplacement(&GURL::Replacements::SetPath, &repl, cur.path);
    SetupReplacement(&GURL::Replacements::SetQuery, &repl, cur.query);
    SetupReplacement(&GURL::Replacements::SetRef, &repl, cur.ref);
    GURL output = url.ReplaceComponents(repl);

    EXPECT_EQ(replace_cases[i].expected, output.spec());

    EXPECT_EQ(output.SchemeIsFileSystem(), output.inner_url() != NULL);
    if (output.SchemeIsFileSystem()) {
      // TODO(mmenke): inner_url()->spec() is currently the same as the spec()
      // for the GURL itself.  This should be fixed.
      // See https://crbug.com/619596
      EXPECT_EQ(replace_cases[i].expected, output.inner_url()->spec());
    }
  }
}

TEST(GURLTest, ClearFragmentOnDataUrl) {
  // http://crbug.com/291747 - a data URL may legitimately have trailing
  // whitespace in the spec after the ref is cleared. Test this does not trigger
  // the Parsed importing validation DCHECK in GURL.
  GURL url(" data: one ? two # three ");

  // By default the trailing whitespace will have been stripped.
  EXPECT_EQ("data: one ? two # three", url.spec());
  GURL::Replacements repl;
  repl.ClearRef();
  GURL url_no_ref = url.ReplaceComponents(repl);

  EXPECT_EQ("data: one ? two ", url_no_ref.spec());

  // Importing a parsed URL via this constructor overload will retain trailing
  // whitespace.
  GURL import_url(url_no_ref.spec(),
                  url_no_ref.parsed_for_possibly_invalid_spec(),
                  url_no_ref.is_valid());
  EXPECT_EQ(url_no_ref, import_url);
  EXPECT_EQ(import_url.query(), " two ");
}

TEST(GURLTest, PathForRequest) {
  struct TestCase {
    const char* input;
    const char* expected;
    const char* inner_expected;
  } cases[] = {
    {"http://www.google.com", "/", NULL},
    {"http://www.google.com/", "/", NULL},
    {"http://www.google.com/foo/bar.html?baz=22", "/foo/bar.html?baz=22", NULL},
    {"http://www.google.com/foo/bar.html#ref", "/foo/bar.html", NULL},
    {"http://www.google.com/foo/bar.html?query#ref", "/foo/bar.html?query", NULL},
    {"filesystem:http://www.google.com/temporary/foo/bar.html?query#ref", "/foo/bar.html?query", "/temporary"},
    {"filesystem:http://www.google.com/temporary/foo/bar.html?query", "/foo/bar.html?query", "/temporary"},
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    std::string path_request = url.PathForRequest();
    EXPECT_EQ(cases[i].expected, path_request);
    EXPECT_EQ(cases[i].inner_expected == NULL, url.inner_url() == NULL);
    if (url.inner_url() && cases[i].inner_expected)
      EXPECT_EQ(cases[i].inner_expected, url.inner_url()->PathForRequest());
  }
}

TEST(GURLTest, EffectiveIntPort) {
  struct PortTest {
    const char* spec;
    int expected_int_port;
  } port_tests[] = {
    // http
    {"http://www.google.com/", 80},
    {"http://www.google.com:80/", 80},
    {"http://www.google.com:443/", 443},

    // https
    {"https://www.google.com/", 443},
    {"https://www.google.com:443/", 443},
    {"https://www.google.com:80/", 80},

    // ftp
    {"ftp://www.google.com/", 21},
    {"ftp://www.google.com:21/", 21},
    {"ftp://www.google.com:80/", 80},

    // gopher
    {"gopher://www.google.com/", 70},
    {"gopher://www.google.com:70/", 70},
    {"gopher://www.google.com:80/", 80},

    // file - no port
    {"file://www.google.com/", PORT_UNSPECIFIED},
    {"file://www.google.com:443/", PORT_UNSPECIFIED},

    // data - no port
    {"data:www.google.com:90", PORT_UNSPECIFIED},
    {"data:www.google.com", PORT_UNSPECIFIED},

    // filesystem - no port
    {"filesystem:http://www.google.com:90/t/foo", PORT_UNSPECIFIED},
    {"filesystem:file:///t/foo", PORT_UNSPECIFIED},
  };

  for (size_t i = 0; i < arraysize(port_tests); i++) {
    GURL url(port_tests[i].spec);
    EXPECT_EQ(port_tests[i].expected_int_port, url.EffectiveIntPort());
  }
}

TEST(GURLTest, IPAddress) {
  struct IPTest {
    const char* spec;
    bool expected_ip;
  } ip_tests[] = {
    {"http://www.google.com/", false},
    {"http://192.168.9.1/", true},
    {"http://192.168.9.1.2/", false},
    {"http://192.168.m.1/", false},
    {"http://2001:db8::1/", false},
    {"http://[2001:db8::1]/", true},
    {"", false},
    {"some random input!", false},
  };

  for (size_t i = 0; i < arraysize(ip_tests); i++) {
    GURL url(ip_tests[i].spec);
    EXPECT_EQ(ip_tests[i].expected_ip, url.HostIsIPAddress());
  }
}

TEST(GURLTest, HostNoBrackets) {
  struct TestCase {
    const char* input;
    const char* expected_host;
    const char* expected_plainhost;
  } cases[] = {
    {"http://www.google.com", "www.google.com", "www.google.com"},
    {"http://[2001:db8::1]/", "[2001:db8::1]", "2001:db8::1"},
    {"http://[::]/", "[::]", "::"},

    // Don't require a valid URL, but don't crash either.
    {"http://[]/", "[]", ""},
    {"http://[x]/", "[x]", "x"},
    {"http://[x/", "[x", "[x"},
    {"http://x]/", "x]", "x]"},
    {"http://[/", "[", "["},
    {"http://]/", "]", "]"},
    {"", "", ""},
  };
  for (size_t i = 0; i < arraysize(cases); i++) {
    GURL url(cases[i].input);
    EXPECT_EQ(cases[i].expected_host, url.host());
    EXPECT_EQ(cases[i].expected_plainhost, url.HostNoBrackets());
    EXPECT_EQ(cases[i].expected_plainhost, url.HostNoBracketsPiece());
  }
}

TEST(GURLTest, DomainIs) {
  GURL url_1("http://google.com/foo");
  EXPECT_TRUE(url_1.DomainIs("google.com"));

  // Subdomain and port are ignored.
  GURL url_2("http://www.google.com:99/foo");
  EXPECT_TRUE(url_2.DomainIs("google.com"));

  // Different top-level domain.
  GURL url_3("http://www.google.com.cn/foo");
  EXPECT_FALSE(url_3.DomainIs("google.com"));

  // Different host name.
  GURL url_4("http://www.iamnotgoogle.com/foo");
  EXPECT_FALSE(url_4.DomainIs("google.com"));

  // The input must be lower-cased otherwise DomainIs returns false.
  GURL url_5("http://www.google.com/foo");
  EXPECT_FALSE(url_5.DomainIs("Google.com"));

  // If the URL is invalid, DomainIs returns false.
  GURL invalid_url("google.com");
  EXPECT_FALSE(invalid_url.is_valid());
  EXPECT_FALSE(invalid_url.DomainIs("google.com"));

  GURL url_with_escape_chars("https://www.,.test");
  EXPECT_TRUE(url_with_escape_chars.is_valid());
  EXPECT_EQ(url_with_escape_chars.host(), "www.%2C.test");
  EXPECT_TRUE(url_with_escape_chars.DomainIs("%2C.test"));
}

TEST(GURLTest, DomainIsTerminatingDotBehavior) {
  // If the host part ends with a dot, it matches input domains
  // with or without a dot.
  GURL url_with_dot("http://www.google.com./foo");
  EXPECT_TRUE(url_with_dot.DomainIs("google.com"));
  EXPECT_TRUE(url_with_dot.DomainIs("google.com."));
  EXPECT_TRUE(url_with_dot.DomainIs(".com"));
  EXPECT_TRUE(url_with_dot.DomainIs(".com."));

  // But, if the host name doesn't end with a dot and the input
  // domain does, then it's considered to not match.
  GURL url_without_dot("http://google.com/foo");
  EXPECT_FALSE(url_without_dot.DomainIs("google.com."));

  // If the URL ends with two dots, it doesn't match.
  GURL url_with_two_dots("http://www.google.com../foo");
  EXPECT_FALSE(url_with_two_dots.DomainIs("google.com"));
}

TEST(GURLTest, DomainIsWithFilesystemScheme) {
  GURL url_1("filesystem:http://www.google.com:99/foo/");
  EXPECT_TRUE(url_1.DomainIs("google.com"));

  GURL url_2("filesystem:http://www.iamnotgoogle.com/foo/");
  EXPECT_FALSE(url_2.DomainIs("google.com"));
}

// Newlines should be stripped from inputs.
TEST(GURLTest, Newlines) {
  // Constructor.
  GURL url_1(" \t ht\ntp://\twww.goo\rgle.com/as\ndf \n ");
  EXPECT_EQ("http://www.google.com/asdf", url_1.spec());
  EXPECT_FALSE(
      url_1.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Relative path resolver.
  GURL url_2 = url_1.Resolve(" \n /fo\to\r ");
  EXPECT_EQ("http://www.google.com/foo", url_2.spec());
  EXPECT_FALSE(
      url_2.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Constructor.
  GURL url_3(" \t ht\ntp://\twww.goo\rgle.com/as\ndf< \n ");
  EXPECT_EQ("http://www.google.com/asdf%3C", url_3.spec());
  EXPECT_TRUE(
      url_3.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Relative path resolver.
  GURL url_4 = url_1.Resolve(" \n /fo\to<\r ");
  EXPECT_EQ("http://www.google.com/foo%3C", url_4.spec());
  EXPECT_TRUE(
      url_4.parsed_for_possibly_invalid_spec().potentially_dangling_markup);

  // Note that newlines are NOT stripped from ReplaceComponents.
}

TEST(GURLTest, IsStandard) {
  GURL a("http:foo/bar");
  EXPECT_TRUE(a.IsStandard());

  GURL b("foo:bar/baz");
  EXPECT_FALSE(b.IsStandard());

  GURL c("foo://bar/baz");
  EXPECT_FALSE(c.IsStandard());

  GURL d("cid:bar@baz");
  EXPECT_FALSE(d.IsStandard());
}

TEST(GURLTest, SchemeIsHTTPOrHTTPS) {
  EXPECT_TRUE(GURL("http://bar/").SchemeIsHTTPOrHTTPS());
  EXPECT_TRUE(GURL("HTTPS://BAR").SchemeIsHTTPOrHTTPS());
  EXPECT_FALSE(GURL("ftp://bar/").SchemeIsHTTPOrHTTPS());
}

TEST(GURLTest, SchemeIsWSOrWSS) {
  EXPECT_TRUE(GURL("WS://BAR/").SchemeIsWSOrWSS());
  EXPECT_TRUE(GURL("wss://bar/").SchemeIsWSOrWSS());
  EXPECT_FALSE(GURL("http://bar/").SchemeIsWSOrWSS());
}

TEST(GURLTest, SchemeIsCryptographic) {
  EXPECT_TRUE(GURL("https://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("HTTPS://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("HtTpS://foo.bar.com/").SchemeIsCryptographic());

  EXPECT_TRUE(GURL("wss://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("WSS://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("WsS://foo.bar.com/").SchemeIsCryptographic());

  EXPECT_TRUE(GURL("https-so://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("HTTPS-SO://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_TRUE(GURL("HtTpS-So://foo.bar.com/").SchemeIsCryptographic());

  EXPECT_FALSE(GURL("http://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_FALSE(GURL("ws://foo.bar.com/").SchemeIsCryptographic());
  EXPECT_FALSE(GURL("http-so://foo.bar.com/").SchemeIsCryptographic());
}

TEST(GURLTest, SchemeIsBlob) {
  EXPECT_TRUE(GURL("BLOB://BAR/").SchemeIsBlob());
  EXPECT_TRUE(GURL("blob://bar/").SchemeIsBlob());
  EXPECT_FALSE(GURL("http://bar/").SchemeIsBlob());
}

TEST(GURLTest, SchemeIsSuborigin) {
  EXPECT_TRUE(GURL("http-so://foo.bar.com/").SchemeIsSuborigin());
  EXPECT_TRUE(GURL("HTTP-SO://foo.bar.com/").SchemeIsSuborigin());
  EXPECT_TRUE(GURL("HtTp-So://foo.bar.com/").SchemeIsSuborigin());
  EXPECT_FALSE(GURL("http://foo.bar.com/").SchemeIsSuborigin());

  EXPECT_TRUE(GURL("https-so://foo.bar.com/").SchemeIsSuborigin());
  EXPECT_TRUE(GURL("HTTPS-SO://foo.bar.com/").SchemeIsSuborigin());
  EXPECT_TRUE(GURL("HtTpS-So://foo.bar.com/").SchemeIsSuborigin());
  EXPECT_FALSE(GURL("https://foo.bar.com/").SchemeIsSuborigin());
}

TEST(GURLTest, ContentAndPathForNonStandardURLs) {
  struct TestCase {
    const char* url;
    const char* expected;
  } cases[] = {
      {"null", ""},
      {"not-a-standard-scheme:this is arbitrary content",
       "this is arbitrary content"},
      {"view-source:http://example.com/path", "http://example.com/path"},
      {"blob:http://example.com/GUID", "http://example.com/GUID"},
      {"blob://http://example.com/GUID", "//http://example.com/GUID"},
      {"blob:http://user:password@example.com/GUID",
       "http://user:password@example.com/GUID"},

      // TODO(mkwst): This seems like a bug. https://crbug.com/513600
      {"filesystem:http://example.com/path", "/"},
  };

  for (const auto& test : cases) {
    GURL url(test.url);
    EXPECT_EQ(test.expected, url.path()) << test.url;
    EXPECT_EQ(test.expected, url.GetContent()) << test.url;
  }
}

TEST(GURLTest, IsAboutBlank) {
  const std::string kAboutBlankUrls[] = {"about:blank", "about:blank?foo",
                                         "about:blank/#foo",
                                         "about:blank?foo#foo"};
  for (const auto& url : kAboutBlankUrls)
    EXPECT_TRUE(GURL(url).IsAboutBlank()) << url;

  const std::string kNotAboutBlankUrls[] = {
      "http:blank",      "about:blan",          "about://blank",
      "about:blank/foo", "about://:8000/blank", "about://foo:foo@/blank",
      "foo@about:blank", "foo:bar@about:blank", "about:blank:8000"};
  for (const auto& url : kNotAboutBlankUrls)
    EXPECT_FALSE(GURL(url).IsAboutBlank()) << url;
}

TEST(GURLTest, EqualsIgnoringRef) {
  const struct {
    const char* url_a;
    const char* url_b;
    bool are_equals;
  } kTestCases[] = {
      // No ref.
      {"http://a.com", "http://a.com", true},
      {"http://a.com", "http://b.com", false},

      // Same Ref.
      {"http://a.com#foo", "http://a.com#foo", true},
      {"http://a.com#foo", "http://b.com#foo", false},

      // Different Refs.
      {"http://a.com#foo", "http://a.com#bar", true},
      {"http://a.com#foo", "http://b.com#bar", false},

      // One has a ref, the other doesn't.
      {"http://a.com#foo", "http://a.com", true},
      {"http://a.com#foo", "http://b.com", false},

      // Empty refs.
      {"http://a.com#", "http://a.com#", true},
      {"http://a.com#", "http://a.com", true},

      // URLs that differ only by their last character.
      {"http://aaa", "http://aab", false},
      {"http://aaa#foo", "http://aab#foo", false},

      // Different size of the part before the ref.
      {"http://123#a", "http://123456#a", false},

      // Blob URLs
      {"blob:http://a.com#foo", "blob:http://a.com#foo", true},
      {"blob:http://a.com#foo", "blob:http://a.com#bar", true},
      {"blob:http://a.com#foo", "blob:http://b.com#bar", false},

      // Filesystem URLs
      {"filesystem:http://a.com#foo", "filesystem:http://a.com#foo", true},
      {"filesystem:http://a.com#foo", "filesystem:http://a.com#bar", true},
      {"filesystem:http://a.com#foo", "filesystem:http://b.com#bar", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "url_a = " << test_case.url_a << std::endl
                 << "url_b = " << test_case.url_b << std::endl);
    // A versus B.
    EXPECT_EQ(test_case.are_equals,
              GURL(test_case.url_a).EqualsIgnoringRef(GURL(test_case.url_b)));
    // B versus A.
    EXPECT_EQ(test_case.are_equals,
              GURL(test_case.url_b).EqualsIgnoringRef(GURL(test_case.url_a)));
  }
}

TEST(GURLTest, LongURLCanonicalize) {
  const std::string kSchemeAndHost = "https://example.com";
  // Reserve space for the scheme, host, and the initial '/'.
  const size_t kPathLength = kMaxURLChars - kSchemeAndHost.size() - 1;

  // URLs that canonicalize to the same size.
  {
    // A canonicalized GURL with a spec that's exactly kMaxURLChars should be
    // valid.
    GURL url(kSchemeAndHost + "/" + std::string(kPathLength, 'a'));
    EXPECT_TRUE(url.is_valid());
  }
  {
    // One that exceeds it in length should not be considered valid though.
    GURL url(kSchemeAndHost + "/" + std::string(kPathLength + 1, 'a'));
    EXPECT_FALSE(url.is_valid());
  }

  // URLs that canonicalize to a smaller size.
  {
    // The canonical URL won't include the port number, since it matches the
    // default port for https.  Eventhough the input spec to canonicalize is
    // longer than kMaxURLChars, the canonicalized result is under the limit, so
    // the url should be valid.
    GURL url(kSchemeAndHost + ":443/" + std::string(kPathLength, 'a'));
    EXPECT_TRUE(url.is_valid());
  }

  // URLs that canonicalize to a bigger size.
  {
    // Raw spaces should be percent encoded, resulting in a longer canonical
    // spec.
    GURL url(kSchemeAndHost + "/a" + std::string(kPathLength - 2, ' ') + "a");
    EXPECT_FALSE(url.is_valid());
  }
}

TEST(GURLTest, LongURLReplaceComponents) {
  const GURL base_url("https://example.com/");
  const size_t kPathLength = kMaxURLChars - base_url.spec().size();

  // Replacements with a boring path (canonicalization will not alter the
  // characters of the path).
  {
    url::Replacements<char> replacements;
    std::string path(kPathLength, 'a');
    replacements.SetPath(path.data(), url::Component(0, path.size()));
    // The resulting url spec should be exactly kMaxURLChars in size.
    GURL url = base_url.ReplaceComponents(replacements);
    EXPECT_TRUE(url.is_valid());
  }

  {
    url::Replacements<char> replacements;
    std::string path(kPathLength + 1, 'a');
    replacements.SetPath(path.data(), url::Component(0, path.size()));
    GURL url = base_url.ReplaceComponents(replacements);
    // The resulting url spec should be exceed kMaxURLChars in size.
    EXPECT_FALSE(url.is_valid());
  }

  // Replacements where the input string to canonicalize exceeds kMaxURLChars,
  // but the canonicalization will bring it under.
  {
    url::Replacements<char> replacements;
    std::string port("443");
    replacements.SetPort(port.data(), url::Component(0, port.size()));
    std::string path(kPathLength, 'a');
    replacements.SetPath(path.data(), url::Component(0, path.size()));
    GURL url = base_url.ReplaceComponents(replacements);
    // The resulting url spec should not exceed kMaxURLChars in size: the
    // canonicalized form won't include the port.
    EXPECT_TRUE(url.is_valid());
  }

  // Replacements where the input string to canonicalize does not exceed
  // kMaxURLChars, but the canonicalization will bring it over.
  {
    url::Replacements<char> replacements;
    std::string path = 'a' + std::string(kPathLength - 2, ' ') + 'a';
    replacements.SetPath(path.data(), url::Component(0, path.size()));
    GURL url = base_url.ReplaceComponents(replacements);
    // The resulting url spec should exceed kMaxURLChars in size, since raw
    // spaces will be percent encoded.
    EXPECT_FALSE(url.is_valid());
  }
}

TEST(GURLTest, LongURLResolve) {
  const GURL base_url("https://example.com/");
  const size_t kPathLength = kMaxURLChars - base_url.spec().size();

  // Resolving with a boring path (canonicalization will not alter the
  // characters of the path).
  {
    std::string path(kPathLength, 'a');
    // The resulting url spec should be exactly kMaxURLChars in size.
    GURL url = base_url.Resolve(path);
    EXPECT_TRUE(url.is_valid());
  }

  {
    std::string path(kPathLength + 1, 'a');
    // The resulting url spec should be exceed kMaxURLChars in size.
    GURL url = base_url.Resolve(path);
    EXPECT_FALSE(url.is_valid());
  }

  // Resolving with a path where the input spec would exceed kMaxURLChars,
  // but the canonicalization will bring it under.
  {
    std::string path = 'a' + std::string(kPathLength * 2, '\n') + 'a';
    GURL url = base_url.Resolve(path);
    // The resulting url spec should not exceed kMaxURLChars in size, since the
    // newlines should be stripped.
    EXPECT_TRUE(url.is_valid());
  }

  // Resolving with a path where the input spec would not exceed kMaxURLChars,
  // but the canonicalization will bring it over.
  {
    std::string path = 'a' + std::string(kPathLength - 2, ' ') + 'a';
    GURL url = base_url.Resolve(path);
    // The resulting url spec should exceed kMaxURLChars in size, since raw
    // spaces will be percent encoded.
    EXPECT_FALSE(url.is_valid());
  }
}

}  // namespace url
