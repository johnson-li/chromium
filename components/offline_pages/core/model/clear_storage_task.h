// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_STORAGE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_STORAGE_TASK_H_

#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/task.h"

namespace base {
class Time;
}  // namespace base

namespace offline_pages {

class ClientPolicyController;
class OfflinePageMetadataStoreSQL;

// This task is responsible for clearing expired temporary pages from metadata
// store and disk. The task needs to be provided with a |last_start_time| in
// order to decide if it's going to do the clearing.
// The callback will provide the time when the task starts, how many pages are
// cleared and a ClearStorageResult.
class ClearStorageTask : public Task {
 public:
  // The name of histogram enum is OfflinePagesClearStorageResult.
  enum class ClearStorageResult {
    SUCCESS,                                // Cleared successfully.
    UNNECESSARY,                            // No expired pages.
    DEPRECATED_EXPIRE_FAILURE,              // Expiration failed. (DEPRECATED)
    DELETE_FAILURE,                         // Deletion failed.
    DEPRECATED_EXPIRE_AND_DELETE_FAILURES,  // Both expiration and deletion
                                            // failed. (DEPRECATED)
    // NOTE: always keep this entry at the end. Add new result types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    RESULT_COUNT,
  };

  // Callback used when calling ClearPagesIfNeeded.
  // base::Time: the starting time of this clear storage attempt.
  // size_t: the number of cleared pages.
  // ClearStorageResult: result of clearing pages in storage.
  typedef base::OnceCallback<
      void(const base::Time&, size_t, ClearStorageResult)>
      ClearStorageCallback;

  ClearStorageTask(OfflinePageMetadataStoreSQL* store,
                   ArchiveManager* archive_manager,
                   ClientPolicyController* policy_controller,
                   const base::Time& last_start_time,
                   const base::Time& clearup_time,
                   ClearStorageCallback callback);
  ~ClearStorageTask() override;

  // Task implementation.
  void Run() override;

 private:
  void OnGetStorageStatsDone(const ArchiveManager::StorageStats& stats);
  void OnClearPagesDone(std::pair<size_t, DeletePageResult> result);
  void InformClearStorageDone(size_t pages_cleared, ClearStorageResult result);

  // The store containing the pages to be cleared. Not owned.
  OfflinePageMetadataStoreSQL* store_;
  // The archive manager owning the archive directories to delete pages from.
  // Not owned.
  ArchiveManager* archive_manager_;
  // The policy controller which is used to determine if a page needs to be
  // cleared. Not owned.
  ClientPolicyController* policy_controller_;
  ClearStorageCallback callback_;
  base::Time last_start_time_;
  base::Time clearup_time_;

  base::WeakPtrFactory<ClearStorageTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ClearStorageTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_STORAGE_TASK_H_
