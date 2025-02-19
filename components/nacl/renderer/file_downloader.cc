// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/file_downloader.h"

#include <utility>

#include "base/callback.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAssociatedURLLoader.h"

namespace nacl {

FileDownloader::FileDownloader(
    std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
    base::File file,
    StatusCallback status_cb,
    ProgressCallback progress_cb)
    : url_loader_(std::move(url_loader)),
      file_(std::move(file)),
      status_cb_(status_cb),
      progress_cb_(progress_cb),
      http_status_code_(-1),
      total_bytes_received_(0),
      total_bytes_to_be_received_(-1),
      status_(SUCCESS) {
  CHECK(!status_cb.is_null());
}

FileDownloader::~FileDownloader() {
}

void FileDownloader::Load(const blink::WebURLRequest& request) {
  url_loader_->LoadAsynchronously(request, this);
}

void FileDownloader::DidReceiveResponse(const blink::WebURLResponse& response) {
  http_status_code_ = response.HttpStatusCode();
  if (http_status_code_ != 200)
    status_ = FAILED;

  // Set -1 if the content length is unknown. Set before issuing callback.
  total_bytes_to_be_received_ = response.ExpectedContentLength();
  if (!progress_cb_.is_null())
    progress_cb_.Run(total_bytes_received_, total_bytes_to_be_received_);
}

void FileDownloader::DidReceiveData(const char* data, int data_length) {
  if (status_ == SUCCESS) {
    if (file_.Write(total_bytes_received_, data, data_length) == -1) {
      status_ = FAILED;
      return;
    }
    total_bytes_received_ += data_length;
    if (!progress_cb_.is_null())
      progress_cb_.Run(total_bytes_received_, total_bytes_to_be_received_);
  }
}

void FileDownloader::DidFinishLoading(double finish_time) {
  if (status_ == SUCCESS) {
    // Seek back to the beginning of the file that was just written so it's
    // easy for consumers to use.
    if (file_.Seek(base::File::FROM_BEGIN, 0) != 0)
      status_ = FAILED;
  }
  status_cb_.Run(status_, std::move(file_), http_status_code_);
  delete this;
}

void FileDownloader::DidFail(const blink::WebURLError& error) {
  status_ = FAILED;
  if (error.domain == blink::WebURLError::Domain::kNet) {
    switch (error.reason) {
      case net::ERR_ACCESS_DENIED:
      case net::ERR_NETWORK_ACCESS_DENIED:
        status_ = ACCESS_DENIED;
        break;
    }
  }

  if (error.is_web_security_violation)
    status_ = ACCESS_DENIED;

  // Delete url_loader to prevent didFinishLoading from being called, which
  // some implementations of blink::WebURLLoader will do after calling didFail.
  url_loader_.reset();

  status_cb_.Run(status_, std::move(file_), http_status_code_);
  delete this;
}

}  // namespace nacl
