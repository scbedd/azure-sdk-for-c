// Copyright (c) Microsoft Corporation. All rights reserved.
// SPDX-License-Identifier: MIT

#include <az_curl.h>

#include <az_http_result.h>
#include <az_action.h>
#include <az_http_header.h>

#include <_az_cfg.h>

/**
 * Appends a zero-terminated string to the given CURL list.
 */
AZ_NODISCARD az_result
az_curl_slist_append(struct curl_slist ** const self, char const * str) {
  AZ_CONTRACT_ARG_NOT_NULL(self);
  AZ_CONTRACT_ARG_NOT_NULL(str);

  struct curl_slist * const p_list = curl_slist_append(*self, str);
  if (p_list == NULL) {
    return AZ_HTTP_ERROR_PAL;
  }
  *self = p_list;
  return AZ_OK;
}

/**
 * Creates a function `az_curl_slist_append` that returns a value of type `az_str_action`.
 */
AZ_ACTION_FUNC(az_curl_slist_append, struct curl_slist *, az_str_action)

/**
 * Appends a header to the given CURL list.
 */
AZ_NODISCARD az_result az_curl_slist_append_header(struct curl_slist ** pp_list, az_pair const header) {
  AZ_CONTRACT_ARG_NOT_NULL(pp_list);

  az_span_emitter const header_span_emitter = az_http_header_emit_span_seq_action(&header);
  az_str_action const curl_slist_append_action = az_curl_slist_append_action(pp_list);

  // the function creates a temporary dynamic zero-terminated string from `header_span_emitter` 
  // and pass it to `curl_slist_append_str` action.
  return az_span_emitter_to_tmp_str(header_span_emitter, curl_slist_append_action);
}