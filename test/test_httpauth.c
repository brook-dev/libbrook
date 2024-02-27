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

#include <string.h>
#include <microhttpd.h>
#include <sagui.h>
#include "sg_httpres.h"
#include "sg_httpauth.h"

static void test__httpauth_new(struct MHD_Connection *con) {
  struct sg_httpres *res = sg__httpres_new(con);
  struct sg_httpauth *auth = sg__httpauth_new(res);
  ASSERT(auth);
  sg__httpauth_free(auth);
  sg__httpres_free(res);
}

static void test__httpauth_free(void) {
  sg__httpauth_free(NULL);
}

static void test__httpauth_dispatch(struct sg_httpauth *auth) {
  const size_t len = 3;

  auth->res->ret = true;
  ASSERT(sg__httpauth_dispatch(auth));
  ASSERT(auth->res->ret == MHD_YES);

  auth->res->ret = false;
  auth->canceled = true;
  ASSERT(!sg__httpauth_dispatch(auth));
  ASSERT(auth->res->ret == MHD_NO);

  auth->res->ret = false;
  auth->canceled = true;
  auth->res->handle =
    MHD_create_response_from_buffer(len, "foo", MHD_RESPMEM_PERSISTENT);
  ASSERT(sg__httpauth_dispatch(auth));
  ASSERT(auth->res->ret == MHD_YES);
  MHD_destroy_response(auth->res->handle);
  auth->res->handle = NULL;

  auth->res->ret = false;
  auth->canceled = false;
  auth->res->handle = NULL;
  ASSERT(!sg__httpauth_dispatch(auth));
  ASSERT(auth->res->ret == MHD_NO);

  auth->res->ret = false;
  auth->canceled = false;
  auth->res->con = NULL;
  auth->res->handle =
    MHD_create_response_from_buffer(len, "foo", MHD_RESPMEM_PERSISTENT);
  ASSERT(!sg__httpauth_dispatch(auth));
  ASSERT(auth->res->ret == MHD_NO);
  MHD_destroy_response(auth->res->handle);
  auth->res->handle = NULL;
}

static void test_httpauth_set_realm(struct sg_httpauth *auth) {
  ASSERT(sg_httpauth_set_realm(NULL, "") == EINVAL);
  ASSERT(sg_httpauth_set_realm(auth, NULL) == EINVAL);

  ASSERT(sg_httpauth_set_realm(auth, "foo") == 0);
  ASSERT(sg_httpauth_set_realm(auth, "foo") == EALREADY);
  ASSERT(sg_httpauth_set_realm(auth, "bar") == EALREADY);
  ASSERT(strcmp(auth->realm, "foo") == 0);
  sg_free(auth->realm);
  auth->realm = NULL;
}

static void test_httpauth_realm(struct sg_httpauth *auth) {
  errno = 0;
  ASSERT(!sg_httpauth_realm(NULL));
  ASSERT(errno == EINVAL);

  ASSERT(sg_httpauth_set_realm(auth, "foo") == 0);
  ASSERT(strcmp(sg_httpauth_realm(auth), "foo") == 0);
  sg_free(auth->realm);
  auth->realm = NULL;
}

static void test_httpauth_deny2(struct sg_httpauth *auth) {
  ASSERT(sg_httpauth_deny2(NULL, "", "", 200) == EINVAL);
  ASSERT(sg_httpauth_deny2(auth, NULL, "", 200) == EINVAL);
  ASSERT(sg_httpauth_deny2(auth, "", NULL, 200) == EINVAL);
  ASSERT(sg_httpauth_deny2(auth, "", "", 99) == EINVAL);
  ASSERT(sg_httpauth_deny2(auth, "", "", 600) == EINVAL);

  ASSERT(sg_httpauth_deny2(auth, MHD_HTTP_HEADER_CONTENT_TYPE, "foo",
                           MHD_HTTP_FORBIDDEN) == 0);
  ASSERT(auth->res->status == MHD_HTTP_FORBIDDEN);
  ASSERT(sg_httpauth_deny2(auth, MHD_HTTP_HEADER_CONTENT_TYPE, "bar", 200) ==
         EALREADY);
  ASSERT(sg_httpauth_deny2(auth, "bar", "foo", 200) == EALREADY);
  ASSERT(strcmp(sg_strmap_get(auth->res->headers, MHD_HTTP_HEADER_CONTENT_TYPE),
                "foo") == 0);
  MHD_destroy_response(auth->res->handle);
  auth->res->handle = NULL;
  sg_strmap_cleanup(&auth->res->headers);
}

static void test_httpauth_deny(struct sg_httpauth *auth) {
  ASSERT(sg_httpauth_deny(NULL, "", "") == EINVAL);
  ASSERT(sg_httpauth_deny(auth, NULL, "") == EINVAL);
  ASSERT(sg_httpauth_deny(auth, "", NULL) == EINVAL);

  ASSERT(sg_httpauth_deny(auth, MHD_HTTP_HEADER_CONTENT_TYPE, "foo") == 0);
  ASSERT(auth->res->status == MHD_HTTP_UNAUTHORIZED);
  ASSERT(sg_httpauth_deny(auth, MHD_HTTP_HEADER_CONTENT_TYPE, "bar") ==
         EALREADY);
  ASSERT(sg_httpauth_deny(auth, "bar", "foo") == EALREADY);
  ASSERT(strcmp(sg_strmap_get(auth->res->headers, MHD_HTTP_HEADER_CONTENT_TYPE),
                "foo") == 0);
  MHD_destroy_response(auth->res->handle);
  auth->res->handle = NULL;
  sg_strmap_cleanup(&auth->res->headers);
}

static void test_httpauth_cancel(struct sg_httpauth *auth) {
  ASSERT(sg_httpauth_cancel(NULL) == EINVAL);

  auth->canceled = false;
  ASSERT(!auth->canceled);
  ASSERT(sg_httpauth_cancel(auth) == 0);
  ASSERT(auth->canceled);
}

static void test_httpauth_usr(struct sg_httpauth *auth) {
  errno = 0;
  ASSERT(!sg_httpauth_usr(NULL));
  ASSERT(errno == EINVAL);

  auth->usr = "foo";
  ASSERT(strcmp(sg_httpauth_usr(auth), "foo") == 0);
}

static void test_httpauth_pwd(struct sg_httpauth *auth) {
  errno = 0;
  ASSERT(!sg_httpauth_pwd(NULL));
  ASSERT(errno == EINVAL);

  auth->pwd = "foo";
  ASSERT(strcmp(sg_httpauth_pwd(auth), "foo") == 0);
}

int main(void) {
  struct sg_httpauth *auth = sg_alloc(sizeof(struct sg_httpauth));
  ASSERT(auth);
  auth->res = sg_alloc(sizeof(struct sg_httpres));
  auth->res->con = sg_alloc(256);
  ASSERT(auth->res);
  ASSERT(auth->res->con);
  test__httpauth_new(auth->res->con);
  test__httpauth_free();
  test__httpauth_dispatch(auth);
  test_httpauth_set_realm(auth);
  test_httpauth_realm(auth);
  test_httpauth_deny2(auth);
  test_httpauth_deny(auth);
  test_httpauth_cancel(auth);
  test_httpauth_usr(auth);
  test_httpauth_pwd(auth);
  sg_free(auth->res->con);
  sg_free(auth->res);
  sg_free(auth);
  return EXIT_SUCCESS;
}
