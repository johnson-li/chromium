// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_field_trial.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/search.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#endif

using metrics::OmniboxEventProto;

class OmniboxFieldTrialTest : public testing::Test {
 public:
  OmniboxFieldTrialTest() {
    ResetFieldTrialList();
#if defined(OS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        omnibox::kZeroSuggestChromeHomePersonalized, false);
#endif
  }

  PrefService* GetPrefs() { return &pref_service_; }

  void ResetFieldTrialList() {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<metrics::SHA1EntropyProvider>("foo")));
    variations::testing::ClearAllVariationParams();
  }

  // Creates and activates a field trial.
  static base::FieldTrial* CreateTestTrial(const std::string& name,
                                           const std::string& group_name) {
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        name, group_name);
    trial->group();
    return trial;
  }

  // Add a field trial disabling ZeroSuggest.
  static void CreateDisableZeroSuggestTrial() {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kZeroSuggestRule)] = "false";
    variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params);
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
  }

  // EXPECT()s that demotions[match_type] exists with value expected_value.
  static void VerifyDemotion(
      const OmniboxFieldTrial::DemotionMultipliers& demotions,
      AutocompleteMatchType::Type match_type,
      float expected_value);

  // EXPECT()s that OmniboxFieldTrial::GetValueForRuleInContext(|rule|,
  // |page_classification|) returns |rule_value|.
  static void ExpectRuleValue(
      const std::string& rule_value,
      const std::string& rule,
      OmniboxEventProto::PageClassification page_classification);

  // EXPECT()s that OmniboxFieldTrial::GetSuggestPollingStrategy returns
  // |expected_from_last_keystroke| and |expected_delay_ms| for the given
  // experiment params. If one the rule values is NULL, the corresponding
  // variation parameter won't be set thus allowing to test the default
  // behavior.
  void VerifySuggestPollingStrategy(
      const char* from_last_keystroke_rule_value,
      const char* polling_delay_ms_rule_value,
      bool expected_from_last_keystroke,
      int expected_delay_ms);

 private:
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxFieldTrialTest);
};

// static
void OmniboxFieldTrialTest::VerifyDemotion(
    const OmniboxFieldTrial::DemotionMultipliers& demotions,
    AutocompleteMatchType::Type match_type,
    float expected_value) {
  OmniboxFieldTrial::DemotionMultipliers::const_iterator demotion_it =
      demotions.find(match_type);
  ASSERT_TRUE(demotion_it != demotions.end());
  EXPECT_FLOAT_EQ(expected_value, demotion_it->second);
}

// static
void OmniboxFieldTrialTest::ExpectRuleValue(
    const std::string& rule_value,
    const std::string& rule,
    OmniboxEventProto::PageClassification page_classification) {
  EXPECT_EQ(rule_value,
            OmniboxFieldTrial::GetValueForRuleInContext(
                rule, page_classification));
}

void OmniboxFieldTrialTest::VerifySuggestPollingStrategy(
    const char* from_last_keystroke_rule_value,
    const char* polling_delay_ms_rule_value,
    bool expected_from_last_keystroke,
    int expected_delay_ms) {
  ResetFieldTrialList();
  std::map<std::string, std::string> params;
  if (from_last_keystroke_rule_value != NULL) {
    params[std::string(
        OmniboxFieldTrial::kMeasureSuggestPollingDelayFromLastKeystrokeRule)] =
        from_last_keystroke_rule_value;
  }
  if (polling_delay_ms_rule_value != NULL) {
    params[std::string(
        OmniboxFieldTrial::kSuggestPollingDelayMsRule)] =
        polling_delay_ms_rule_value;
  }
  ASSERT_TRUE(variations::AssociateVariationParams(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  bool from_last_keystroke;
  int delay_ms;
  OmniboxFieldTrial::GetSuggestPollingStrategy(&from_last_keystroke, &delay_ms);
  EXPECT_EQ(expected_from_last_keystroke, from_last_keystroke);
  EXPECT_EQ(expected_delay_ms, delay_ms);
}

// Test if GetDisabledProviderTypes() properly parses various field trial
// group names.
TEST_F(OmniboxFieldTrialTest, GetDisabledProviderTypes) {
  EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());

  {
    SCOPED_TRACE("Valid field trial, missing param.");
    ResetFieldTrialList();
    std::map<std::string, std::string> params;
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
  }

  {
    SCOPED_TRACE("Valid field trial, empty param value.");
    ResetFieldTrialList();
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDisableProvidersRule)] = "";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
  }

  {
    SCOPED_TRACE("Valid field trial, invalid param value.");
    ResetFieldTrialList();
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDisableProvidersRule)] = "aaa";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_EQ(0, OmniboxFieldTrial::GetDisabledProviderTypes());
  }

  {
    SCOPED_TRACE("Valid field trial and param.");
    ResetFieldTrialList();
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDisableProvidersRule)] = "12321";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_EQ(12321, OmniboxFieldTrial::GetDisabledProviderTypes());
  }
}

// Test if InZeroSuggestFieldTrial() properly parses various field trial
// group names.
TEST_F(OmniboxFieldTrialTest, ZeroSuggestFieldTrial) {
  // Default ZeroSuggest setting depends on OS.
#if defined(OS_IOS)
  EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
#else
  EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
#endif

  {
    SCOPED_TRACE("Disable ZeroSuggest.");
    ResetFieldTrialList();
    CreateDisableZeroSuggestTrial();
    EXPECT_FALSE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
  }

  {
    SCOPED_TRACE("Bundled field trial parameters.");
    ResetFieldTrialList();
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kZeroSuggestRule)] = "true";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
#if defined(OS_ANDROID)
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
#else
    EXPECT_FALSE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
#endif

    ResetFieldTrialList();
    params[std::string(OmniboxFieldTrial::kZeroSuggestVariantRule)] =
        "MostVisited";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));

    ResetFieldTrialList();
    params.erase(std::string(OmniboxFieldTrial::kZeroSuggestVariantRule));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
#if defined(OS_ANDROID)
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
#else
    EXPECT_FALSE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
#endif
  }
#if defined(OS_ANDROID)
  {
    SCOPED_TRACE("Chrome Home personalized suggestions.");
    ResetFieldTrialList();
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kZeroSuggestRule)] = "true";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
    EXPECT_TRUE(OmniboxFieldTrial::InZeroSuggestFieldTrial());
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
    EXPECT_FALSE(
        OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial(GetPrefs()));

    GetPrefs()->SetBoolean(omnibox::kZeroSuggestChromeHomePersonalized, true);
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
    EXPECT_FALSE(
        OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial(GetPrefs()));

    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        omnibox::kAndroidChromeHomePersonalizedSuggestions);
    EXPECT_FALSE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial(GetPrefs()));

    GetPrefs()->SetBoolean(omnibox::kZeroSuggestChromeHomePersonalized, false);
    EXPECT_TRUE(
        OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial(GetPrefs()));
    EXPECT_FALSE(
        OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial(GetPrefs()));
  }
#endif
}

TEST_F(OmniboxFieldTrialTest, GetDemotionsByTypeWithFallback) {
  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":1:*"] =
        "1:50,2:0";
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":3:*"] =
        "5:100";
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":*:*"] = "1:25";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
  OmniboxFieldTrial::DemotionMultipliers demotions_by_type;
  OmniboxFieldTrial::GetDemotionsByType(
      OmniboxEventProto::NTP, &demotions_by_type);
  ASSERT_EQ(2u, demotions_by_type.size());
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::HISTORY_URL, 0.5);
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::HISTORY_TITLE, 0.0);
  OmniboxFieldTrial::GetDemotionsByType(
      OmniboxEventProto::HOME_PAGE, &demotions_by_type);
  ASSERT_EQ(1u, demotions_by_type.size());
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::NAVSUGGEST, 1.0);
  OmniboxFieldTrial::GetDemotionsByType(
      OmniboxEventProto::BLANK, &demotions_by_type);
  ASSERT_EQ(1u, demotions_by_type.size());
  VerifyDemotion(demotions_by_type, AutocompleteMatchType::HISTORY_URL, 0.25);
}

TEST_F(OmniboxFieldTrialTest, GetValueForRuleInContext) {
  {
    std::map<std::string, std::string> params;
    // Rule 1 has some exact matches and fallbacks at every level.
    params["rule1:1:0"] = "rule1-1-0-value";  // NTP
    params["rule1:3:0"] = "rule1-3-0-value";  // HOME_PAGE
    params["rule1:4:1"] = "rule1-4-1-value";  // OTHER
    params["rule1:4:*"] = "rule1-4-*-value";  // OTHER
    params["rule1:*:1"] = "rule1-*-1-value";  // global
    params["rule1:*:*"] = "rule1-*-*-value";  // global
    // Rule 2 has no exact matches but has fallbacks.
    params["rule2:*:0"] = "rule2-*-0-value";  // global
    params["rule2:1:*"] = "rule2-1-*-value";  // NTP
    params["rule2:*:*"] = "rule2-*-*-value";  // global
    // Rule 3 has only a global fallback.
    params["rule3:*:*"] = "rule3-*-*-value";  // global
    // Rule 4 has an exact match but no fallbacks.
    params["rule4:4:0"] = "rule4-4-0-value";  // OTHER
    // Add a malformed rule to make sure it doesn't screw things up.
    params["unrecognized"] = "unrecognized-value";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }

  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  if (search::IsInstantExtendedAPIEnabled()) {
    // Tests with Instant Extended enabled.
    // Tests for rule 1.
    ExpectRuleValue("rule1-4-1-value",
                    "rule1", OmniboxEventProto::OTHER);    // exact match
    ExpectRuleValue("rule1-*-1-value",
                    "rule1", OmniboxEventProto::BLANK);    // partial fallback
    ExpectRuleValue("rule1-*-1-value",
                    "rule1",
                    OmniboxEventProto::NTP);               // partial fallback

    // Tests for rule 2.
    ExpectRuleValue("rule2-1-*-value",
                    "rule2",
                    OmniboxEventProto::NTP);               // partial fallback
    ExpectRuleValue("rule2-*-*-value",
                    "rule2", OmniboxEventProto::OTHER);    // global fallback

    // Tests for rule 3.
    ExpectRuleValue("rule3-*-*-value",
                    "rule3",
                    OmniboxEventProto::HOME_PAGE);         // global fallback
    ExpectRuleValue("rule3-*-*-value",
                    "rule3",
                    OmniboxEventProto::OTHER);             // global fallback

    // Tests for rule 4.
    ExpectRuleValue("",
                    "rule4",
                    OmniboxEventProto::BLANK);             // no global fallback
    ExpectRuleValue("",
                    "rule4",
                    OmniboxEventProto::HOME_PAGE);         // no global fallback

    // Tests for rule 5 (a missing rule).
    ExpectRuleValue("",
                    "rule5", OmniboxEventProto::OTHER);    // no rule at all
  } else {
    // Tests for rule 1.
    ExpectRuleValue("rule1-1-0-value",
                    "rule1", OmniboxEventProto::NTP);      // exact match
    ExpectRuleValue("rule1-1-0-value",
                    "rule1", OmniboxEventProto::NTP);      // exact match
    ExpectRuleValue("rule1-*-*-value",
                    "rule1", OmniboxEventProto::BLANK);    // fallback to global
    ExpectRuleValue("rule1-3-0-value",
                    "rule1",
                    OmniboxEventProto::HOME_PAGE);         // exact match
    ExpectRuleValue("rule1-4-*-value",
                    "rule1", OmniboxEventProto::OTHER);    // partial fallback
    // Tests for rule 2.
    ExpectRuleValue("rule2-*-0-value",
                    "rule2",
                    OmniboxEventProto::HOME_PAGE);         // partial fallback
    ExpectRuleValue("rule2-*-0-value",
                    "rule2", OmniboxEventProto::OTHER);    // partial fallback

    // Tests for rule 3.
    ExpectRuleValue("rule3-*-*-value",
                    "rule3",
                    OmniboxEventProto::HOME_PAGE);         // fallback to global
    ExpectRuleValue("rule3-*-*-value",
                    "rule3", OmniboxEventProto::OTHER);    // fallback to global

    // Tests for rule 4.
    ExpectRuleValue("",
                    "rule4", OmniboxEventProto::BLANK);    // no global fallback
    ExpectRuleValue("",
                    "rule4",
                    OmniboxEventProto::HOME_PAGE);         // no global fallback
    ExpectRuleValue("rule4-4-0-value",
                    "rule4", OmniboxEventProto::OTHER);    // exact match

    // Tests for rule 5 (a missing rule).
    ExpectRuleValue("",
                    "rule5", OmniboxEventProto::OTHER);    // no rule at all
  }
}

TEST_F(OmniboxFieldTrialTest, HUPNewScoringFieldTrial) {
  {
    std::map<std::string, std::string> params;
    params[std::string(
        OmniboxFieldTrial::kHUPNewScoringTypedCountRelevanceCapParam)] = "56";
    params[std::string(
        OmniboxFieldTrial::kHUPNewScoringTypedCountHalfLifeTimeParam)] = "77";
    params[std::string(
        OmniboxFieldTrial::kHUPNewScoringTypedCountScoreBucketsParam)] =
        "0.2:25,0.1:1001,2.3:777";
    params[std::string(
        OmniboxFieldTrial::kHUPNewScoringVisitedCountRelevanceCapParam)] = "11";
    params[std::string(
        OmniboxFieldTrial::kHUPNewScoringVisitedCountHalfLifeTimeParam)] = "31";
    params[std::string(
        OmniboxFieldTrial::kHUPNewScoringVisitedCountScoreBucketsParam)] =
        "5:300,0:200";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  HUPScoringParams scoring_params;
  OmniboxFieldTrial::GetExperimentalHUPScoringParams(&scoring_params);
  EXPECT_EQ(56, scoring_params.typed_count_buckets.relevance_cap());
  EXPECT_EQ(77, scoring_params.typed_count_buckets.half_life_days());
  ASSERT_EQ(3u, scoring_params.typed_count_buckets.buckets().size());
  EXPECT_EQ(std::make_pair(2.3, 777),
            scoring_params.typed_count_buckets.buckets()[0]);
  EXPECT_EQ(std::make_pair(0.2, 25),
            scoring_params.typed_count_buckets.buckets()[1]);
  EXPECT_EQ(std::make_pair(0.1, 1001),
            scoring_params.typed_count_buckets.buckets()[2]);
  EXPECT_EQ(11, scoring_params.visited_count_buckets.relevance_cap());
  EXPECT_EQ(31, scoring_params.visited_count_buckets.half_life_days());
  ASSERT_EQ(2u, scoring_params.visited_count_buckets.buckets().size());
  EXPECT_EQ(std::make_pair(5.0, 300),
            scoring_params.visited_count_buckets.buckets()[0]);
  EXPECT_EQ(std::make_pair(0.0, 200),
            scoring_params.visited_count_buckets.buckets()[1]);
}

TEST_F(OmniboxFieldTrialTest, HUPNewScoringFieldTrialWithDecayFactor) {
  {
    std::map<std::string, std::string> params;
    params[OmniboxFieldTrial::kHUPNewScoringTypedCountHalfLifeTimeParam] = "10";
    params[OmniboxFieldTrial::kHUPNewScoringTypedCountUseDecayFactorParam] =
        "1";
    params[OmniboxFieldTrial::kHUPNewScoringTypedCountScoreBucketsParam] =
        "0.1:100,0.5:500,1.0:1000";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  HUPScoringParams scoring_params;
  OmniboxFieldTrial::GetExperimentalHUPScoringParams(&scoring_params);
  EXPECT_EQ(10, scoring_params.typed_count_buckets.half_life_days());
  ASSERT_EQ(3u, scoring_params.typed_count_buckets.buckets().size());
  ASSERT_TRUE(scoring_params.typed_count_buckets.use_decay_factor());
}

TEST_F(OmniboxFieldTrialTest, HalfLifeTimeDecay) {
  HUPScoringParams::ScoreBuckets buckets;

  // No decay by default.
  EXPECT_EQ(1.0, buckets.HalfLifeTimeDecay(base::TimeDelta::FromDays(7)));

  buckets.set_half_life_days(7);
  EXPECT_EQ(0.5, buckets.HalfLifeTimeDecay(base::TimeDelta::FromDays(7)));
  EXPECT_EQ(0.25, buckets.HalfLifeTimeDecay(base::TimeDelta::FromDays(14)));
  EXPECT_EQ(1.0, buckets.HalfLifeTimeDecay(base::TimeDelta::FromDays(0)));
  EXPECT_EQ(1.0, buckets.HalfLifeTimeDecay(base::TimeDelta::FromDays(-1)));
}

TEST_F(OmniboxFieldTrialTest, DisableResultsCaching) {
  EXPECT_FALSE(OmniboxFieldTrial::DisableResultsCaching());

  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDisableResultsCachingRule)] = "true";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
    base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

    EXPECT_TRUE(OmniboxFieldTrial::DisableResultsCaching());
  }
}

TEST_F(OmniboxFieldTrialTest, GetSuggestPollingStrategy) {
  // Invalid params.
  VerifySuggestPollingStrategy(
      "", "", false,
      OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs);
  VerifySuggestPollingStrategy(
      "foo", "-1", false,
      OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs);
  VerifySuggestPollingStrategy(
      "TRUE", "xyz", false,
      OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs);

  // Default values.
  VerifySuggestPollingStrategy(
      NULL, NULL, false,
      OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs);

  // Valid params.
  VerifySuggestPollingStrategy("true", "50", true, 50);
  VerifySuggestPollingStrategy(NULL, "35", false, 35);
  VerifySuggestPollingStrategy(
      "true", NULL, true,
      OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs);
}
