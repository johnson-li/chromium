// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/billing_address_selection_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/payments/payment_request_unittest_base.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetNameLabelFromAutofillProfile;
using ::payment_request_util::GetBillingAddressLabelFromAutofillProfile;
using ::payment_request_util::GetPhoneNumberLabelFromAutofillProfile;
using ::payment_request_util::GetAddressNotificationLabelFromAutofillProfile;
}  // namespace

class FakePaymentsProfileComparator
    : public payments::PaymentsProfileComparator {
 public:
  FakePaymentsProfileComparator(const std::string& app_locale,
                                const payments::PaymentOptionsProvider& options)
      : PaymentsProfileComparator(app_locale, options) {}
};

class PaymentRequestBillingAddressSelectionMediatorTest
    : public PaymentRequestUnitTestBase,
      public PlatformTest {
 protected:
  void SetUp() override {
    PaymentRequestUnitTestBase::SetUp();

    AddAutofillProfile(std::move(autofill::test::GetFullProfile()));
    AddAutofillProfile(std::move(autofill::test::GetFullProfile2()));
    AddAutofillProfile(std::move(autofill::test::GetIncompleteProfile1()));
    AddAutofillProfile(std::move(autofill::test::GetIncompleteProfile2()));

    CreateTestPaymentRequest();

    profile_comparator_ = base::MakeUnique<FakePaymentsProfileComparator>(
        payment_request()->GetApplicationLocale(), *payment_request());
    payment_request()->SetProfileComparator(profile_comparator_.get());

    mediator_ = [[BillingAddressSelectionMediator alloc]
        initWithPaymentRequest:payment_request()
        selectedBillingProfile:payment_request()->billing_profiles()[1]];
  }

  void TearDown() override { PaymentRequestUnitTestBase::TearDown(); }

  BillingAddressSelectionMediator* mediator() const { return mediator_; }

  BillingAddressSelectionMediator* mediator_;

  std::unique_ptr<payments::PaymentsProfileComparator> profile_comparator_;
};

// Tests that the expected selectable items are created and that the index of
// the selected item is properly set.
TEST_F(PaymentRequestBillingAddressSelectionMediatorTest, TestSelectableItems) {
  NSArray<CollectionViewItem*>* selectable_items = [mediator() selectableItems];

  ASSERT_EQ(4U, selectable_items.count);

  // The second item must be selected.
  EXPECT_EQ(1U, mediator().selectedItemIndex);

  CollectionViewItem* item_1 = [[mediator() selectableItems] objectAtIndex:0];
  DCHECK([item_1 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_1 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_1);
  EXPECT_TRUE([profile_item_1.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.address
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_TRUE([profile_item_1.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[0])]);
  EXPECT_EQ(nil, profile_item_1.email);
  EXPECT_EQ(nil, profile_item_1.notification);
  EXPECT_TRUE(profile_item_1.complete);

  CollectionViewItem* item_2 = [[mediator() selectableItems] objectAtIndex:1];
  DCHECK([item_2 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_2 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_2);
  EXPECT_TRUE([profile_item_2.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.address
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[1])]);
  EXPECT_TRUE([profile_item_2.phoneNumber
      isEqualToString:GetPhoneNumberLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[1])]);
  EXPECT_EQ(nil, profile_item_2.email);
  EXPECT_EQ(nil, profile_item_2.notification);
  EXPECT_TRUE(profile_item_2.complete);

  CollectionViewItem* item_3 = [[mediator() selectableItems] objectAtIndex:2];
  DCHECK([item_3 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_3 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_3);
  EXPECT_TRUE([profile_item_3.name
      isEqualToString:GetNameLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[2])]);
  EXPECT_TRUE([profile_item_3.address
      isEqualToString:GetBillingAddressLabelFromAutofillProfile(
                          *payment_request()->billing_profiles()[2])]);
  EXPECT_EQ(nil, profile_item_3.phoneNumber);
  EXPECT_EQ(nil, profile_item_3.email);
  EXPECT_TRUE([profile_item_3.notification
      isEqualToString:GetAddressNotificationLabelFromAutofillProfile(
                          *payment_request(),
                          *payment_request()->billing_profiles()[2])]);
  EXPECT_FALSE(profile_item_3.complete);

  CollectionViewItem* item_4 = [[mediator() selectableItems] objectAtIndex:3];
  DCHECK([item_4 isKindOfClass:[AutofillProfileItem class]]);
  AutofillProfileItem* profile_item_4 =
      base::mac::ObjCCastStrict<AutofillProfileItem>(item_4);
  EXPECT_EQ(nil, profile_item_4.name);
  EXPECT_EQ(nil, profile_item_4.address);
  EXPECT_EQ(nil, profile_item_4.phoneNumber);
  EXPECT_EQ(nil, profile_item_4.email);
  EXPECT_TRUE([profile_item_4.notification
      isEqualToString:GetAddressNotificationLabelFromAutofillProfile(
                          *payment_request(),
                          *payment_request()->billing_profiles()[3])]);
  EXPECT_FALSE(profile_item_4.complete);
}

// Tests that the index of the selected item is as expected when there is no
// selected billing profile.
TEST_F(PaymentRequestBillingAddressSelectionMediatorTest, TestNoSelectedItem) {
  mediator_ = [[BillingAddressSelectionMediator alloc]
      initWithPaymentRequest:payment_request()
      selectedBillingProfile:nil];

  NSArray<CollectionViewItem*>* selectable_items = [mediator() selectableItems];

  ASSERT_EQ(4U, selectable_items.count);

  // The selected item index must be invalid.
  EXPECT_EQ(NSUIntegerMax, mediator().selectedItemIndex);
}
