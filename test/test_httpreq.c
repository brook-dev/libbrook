/*                         _
 *   ___  __ _  __ _ _   _(_)
 *  / __|/ _` |/ _` | | | | |
 *  \__ \ (_| | (_| | |_| | |
 *  |___/\__,_|\__, |\__,_|_|
 *             |___/
 *
 * Cross-platform library which helps to develop web servers or frameworks.
 *
 * Copyright (C) 2016-2024 Silvio Clecio <silvioprog@gmail.com>
 *
 * Sagui library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Sagui library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Sagui library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "sg_assert.h"

#include <stdlib.h>
#include <string.h>
#include "sg_httpreq.h"
#include <sagui.h>

static void test__httpreq_new(struct MHD_Connection *con,
                              struct sg_httpsrv *srv) {
  struct sg_httpreq *req = sg__httpreq_new(srv, con, "abc", "def", "ghi");
  ASSERT(req);
  ASSERT(req->srv == srv);
  ASSERT(strcmp(req->version, "abc") == 0);
  ASSERT(strcmp(req->method, "def") == 0);
  ASSERT(strcmp(req->path, "ghi") == 0);
  sg__httpreq_free(req);
}

static void test__httpreq_free(void) {
  sg__httpreq_free(NULL);
}

static void dummy_httpreq_cb(void *cls, struct sg_httpreq *req,
                             struct sg_httpres *res) {
  (void) cls;
  (void) req;
  (void) res;
}

static void test_httpreq_srv(struct sg_httpreq *req) {
  struct sg_httpsrv *srv;
  errno = 0;
  ASSERT(!sg_httpreq_srv(NULL));
  ASSERT(errno == EINVAL);

  ASSERT(sg_httpreq_srv(req));

  errno = 0;
  req->srv = NULL;
  ASSERT(!sg_httpreq_srv(req));
  ASSERT(errno == 0);

  srv = sg_httpsrv_new(dummy_httpreq_cb, NULL);
  req->srv = srv;
  ASSERT(sg_httpreq_srv(req) == srv);
  sg_httpsrv_free(srv);
  ASSERT(errno == 0);
}

static void test_httpreq_headers(struct sg_httpreq *req) {
  struct sg_strmap **headers;
  errno = 0;
  ASSERT(!sg_httpreq_headers(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->headers = NULL;
  ASSERT(sg_httpreq_headers(req));
  ASSERT(errno == 0);
  headers = sg_httpreq_headers(req);
  ASSERT(headers);
  ASSERT(sg_strmap_count(*headers) == 0);
  sg_strmap_add(&req->headers, "foo", "bar");
  sg_strmap_add(&req->headers, "abc", "123");
  ASSERT(sg_strmap_count(*headers) == 2);
  ASSERT(strcmp(sg_strmap_get(*headers, "foo"), "bar") == 0);
  ASSERT(strcmp(sg_strmap_get(*headers, "abc"), "123") == 0);
}

static void test_httpreq_cookies(struct sg_httpreq *req) {
  struct sg_strmap **cookies;
  errno = 0;
  ASSERT(!sg_httpreq_cookies(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->cookies = NULL;
  ASSERT(sg_httpreq_cookies(req));
  ASSERT(errno == 0);
  cookies = sg_httpreq_cookies(req);
  ASSERT(cookies);
  ASSERT(sg_strmap_count(*cookies) == 0);
  sg_strmap_add(&req->cookies, "foo", "bar");
  sg_strmap_add(&req->cookies, "abc", "123");
  ASSERT(sg_strmap_count(*cookies) == 2);
  ASSERT(strcmp(sg_strmap_get(*cookies, "foo"), "bar") == 0);
  ASSERT(strcmp(sg_strmap_get(*cookies, "abc"), "123") == 0);
}

static void test_httpreq_params(struct sg_httpreq *req) {
  struct sg_strmap **params;
  errno = 0;
  ASSERT(!sg_httpreq_params(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->params = NULL;
  ASSERT(sg_httpreq_params(req));
  ASSERT(errno == 0);
  params = sg_httpreq_params(req);
  ASSERT(params);
  ASSERT(sg_strmap_count(*params) == 0);
  sg_strmap_add(&req->params, "foo", "bar");
  sg_strmap_add(&req->params, "abc", "123");
  ASSERT(sg_strmap_count(*params) == 2);
  ASSERT(strcmp(sg_strmap_get(*params, "foo"), "bar") == 0);
  ASSERT(strcmp(sg_strmap_get(*params, "abc"), "123") == 0);
}

static void test_httpreq_fields(struct sg_httpreq *req) {
  struct sg_strmap **fields;
  errno = 0;
  ASSERT(!sg_httpreq_fields(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->fields = NULL;
  ASSERT(sg_httpreq_fields(req));
  ASSERT(errno == 0);
  fields = sg_httpreq_fields(req);
  ASSERT(fields);
  ASSERT(sg_strmap_count(*fields) == 0);
  sg_strmap_add(&req->fields, "foo", "bar");
  sg_strmap_add(&req->fields, "abc", "123");
  ASSERT(sg_strmap_count(*fields) == 2);
  ASSERT(strcmp(sg_strmap_get(*fields, "foo"), "bar") == 0);
  ASSERT(strcmp(sg_strmap_get(*fields, "abc"), "123") == 0);
}

static void test_httpreq_version(struct sg_httpreq *req) {
  errno = 0;
  ASSERT(!sg_httpreq_version(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->version = NULL;
  ASSERT(!sg_httpreq_version(req));
  ASSERT(errno == 0);
  req->version = "1.0";
  ASSERT(strcmp(sg_httpreq_version(req), "1.0") == 0);
  req->version = "1.1";
  ASSERT(strcmp(sg_httpreq_version(req), "1.1") == 0);
}

static void test_httpreq_method(struct sg_httpreq *req) {
  errno = 0;
  ASSERT(!sg_httpreq_method(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->method = NULL;
  ASSERT(!sg_httpreq_method(req));
  ASSERT(errno == 0);
  req->method = "GET";
  ASSERT(strcmp(sg_httpreq_method(req), "GET") == 0);
  req->method = "POST";
  ASSERT(strcmp(sg_httpreq_method(req), "POST") == 0);
}

static void test_httpreq_path(struct sg_httpreq *req) {
  errno = 0;
  ASSERT(!sg_httpreq_path(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->path = NULL;
  ASSERT(!sg_httpreq_path(req));
  ASSERT(errno == 0);
  req->path = "/foo";
  ASSERT(strcmp(sg_httpreq_path(req), "/foo") == 0);
  req->path = "/bar";
  ASSERT(strcmp(sg_httpreq_path(req), "/bar") == 0);
}

static void test_httpreq_payload(struct sg_httpreq *req) {
  struct sg_str *old_payload;
  errno = 0;
  ASSERT(!sg_httpreq_payload(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  old_payload = req->payload;
  req->payload = NULL;
  ASSERT(!sg_httpreq_payload(req));
  req->payload = old_payload;
  ASSERT(errno == 0);
  ASSERT(sg_str_length(sg_httpreq_payload(req)) == 0);
  sg_str_printf(sg_httpreq_payload(req), "%s", "abc");
  ASSERT(strcmp(sg_str_content(sg_httpreq_payload(req)), "abc") == 0);
  sg_str_printf(sg_httpreq_payload(req), "%d", 123);
  ASSERT(strcmp(sg_str_content(sg_httpreq_payload(req)), "abc123") == 0);
}

static void test_httpreq_is_uploading(struct sg_httpreq *req) {
  errno = 0;
  ASSERT(!sg_httpreq_is_uploading(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->is_uploading = false;
  ASSERT(!sg_httpreq_is_uploading(req));
  ASSERT(errno == 0);
  req->is_uploading = true;
  ASSERT(sg_httpreq_is_uploading(req));
  ASSERT(errno == 0);
}

static void test_httpreq_uploads(struct sg_httpreq *req) {
  struct sg_httpupld *tmp;
  errno = 0;
  ASSERT(!sg_httpreq_uploads(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  req->uplds = NULL;
  ASSERT(!sg_httpreq_uploads(req));
  ASSERT(errno == 0);
  req->curr_upld = sg_alloc(sizeof(struct sg_httpupld));
  LL_APPEND(req->uplds, req->curr_upld);
  req->curr_upld->name = "foo";
  ASSERT(strcmp(sg_httpupld_name(req->curr_upld), "foo") == 0);
  req->curr_upld = sg_alloc(sizeof(struct sg_httpupld));
  LL_APPEND(req->uplds, req->curr_upld);
  req->curr_upld->name = "bar";
  ASSERT(strcmp(sg_httpupld_name(req->curr_upld), "bar") == 0);
  LL_FOREACH_SAFE(req->uplds, req->curr_upld, tmp) {
    LL_DELETE(req->uplds, req->curr_upld);
    sg_free(req->curr_upld);
  }
}

static void test_httpreq_client(void) {
  errno = 0;
  ASSERT(!sg_httpreq_client(NULL));
  ASSERT(errno == EINVAL);
  /* more tests in `test_httpsrv_tls_curl.c`. */
}

#ifdef SG_HTTPS_SUPPORT

static void test_httpreq_tls_session(void) {
  errno = 0;
  ASSERT(!sg_httpreq_tls_session(NULL));
  ASSERT(errno == EINVAL);
  /* more tests in `test_httpsrv_tls_curl.c`. */
}

#endif /* SG_HTTPS_SUPPORT */

static void test_httpreq_isolate(struct sg_httpreq *req) {
  ASSERT(sg_httpreq_isolate(NULL, dummy_httpreq_cb, NULL) == EINVAL);
  ASSERT(sg_httpreq_isolate(req, NULL, NULL) == EINVAL);
  /* more tests in `test_httpsrv_curl.c`. */
}

static void test_httpreq_set_user_data(struct sg_httpreq *req) {
  const char *dummy = "foo";
  ASSERT(sg_httpreq_set_user_data(NULL, (void *) dummy) == EINVAL);

  ASSERT(sg_httpreq_set_user_data(req, (void *) dummy) == 0);
  ASSERT(strcmp(sg_httpreq_user_data(req), "foo") == 0);
  dummy = "bar";
  ASSERT(sg_httpreq_set_user_data(req, (void *) dummy) == 0);
  ASSERT(strcmp(sg_httpreq_user_data(req), "bar") == 0);
}

static void test_httpreq_user_data(struct sg_httpreq *req) {
  errno = 0;
  ASSERT(!sg_httpreq_user_data(NULL));
  ASSERT(errno == EINVAL);

  errno = 0;
  sg_httpreq_set_user_data(req, NULL);
  ASSERT(!sg_httpreq_user_data(req));
  ASSERT(errno == 0);
  sg_httpreq_set_user_data(req, "foo");
  ASSERT(strcmp(sg_httpreq_user_data(req), "foo") == 0);
  sg_httpreq_set_user_data(req, "bar");
  ASSERT(strcmp(sg_httpreq_user_data(req), "bar") == 0);
}

int main(void) {
  struct sg_httpsrv *srv = sg_httpsrv_new(dummy_httpreq_cb, NULL);
  struct MHD_Connection *con = sg_alloc(256);
  struct sg_httpreq *req = sg__httpreq_new(srv, con, NULL, NULL, NULL);
  test__httpreq_new(con, srv);
  test__httpreq_free();
  test_httpreq_srv(req);
  test_httpreq_headers(req);
  test_httpreq_cookies(req);
  test_httpreq_params(req);
  test_httpreq_fields(req);
  test_httpreq_version(req);
  test_httpreq_method(req);
  test_httpreq_path(req);
  test_httpreq_payload(req);
  test_httpreq_is_uploading(req);
  test_httpreq_uploads(req);
  test_httpreq_client();
#ifdef SG_HTTPS_SUPPORT
  test_httpreq_tls_session();
#endif /* SG_HTTPS_SUPPORT */
  test_httpreq_isolate(req);
  test_httpreq_set_user_data(req);
  test_httpreq_user_data(req);
  sg__httpreq_free(req);
  sg_httpsrv_free(srv);
  sg_free(con);
  return EXIT_SUCCESS;
}
