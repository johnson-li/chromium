// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_SESSION_DURATION_UPDATER_FACTORY_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_SESSION_DURATION_UPDATER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace feature_engagement {

class SessionDurationUpdater;

// SessionDurationUpdaterFactory is the main client class for interaction with
// the SessionDurationUpdater component.
class SessionDurationUpdaterFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of SessionDurationUpdaterFactory.
  static SessionDurationUpdaterFactory* GetInstance();

  // Returns the SessionDurationUpdater associated with the profile.
  SessionDurationUpdater* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<SessionDurationUpdaterFactory>;

  SessionDurationUpdaterFactory();
  ~SessionDurationUpdaterFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SessionDurationUpdaterFactory);
};

}  // namespace feature_engagement

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_SESSION_DURATION_UPDATER_FACTORY_H_
