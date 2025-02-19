// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_
#define CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/WebKit/public/platform/InterfaceRegistry.h"

namespace content {

class BlinkInterfaceRegistryImpl final : public blink::InterfaceRegistry {
 public:
  explicit BlinkInterfaceRegistryImpl(
      base::WeakPtr<service_manager::BinderRegistry> interface_registry);
  ~BlinkInterfaceRegistryImpl();

  // blink::InterfaceRegistry override.
  void AddInterface(const char* name,
                    const blink::InterfaceFactory& factory) override;

 private:
  const base::WeakPtr<service_manager::BinderRegistry> interface_registry_;

  DISALLOW_COPY_AND_ASSIGN(BlinkInterfaceRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BLINK_INTERFACE_REGISTRY_IMPL_H_
