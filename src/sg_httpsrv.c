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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif /* _WIN32 */
#include "sg_macros.h"
#include "utlist.h"
#include "microhttpd.h"
#include "sagui.h"
#include "sg_utils.h"
#include "sg_httpauth.h"
#include "sg_httpreq.h"
#include "sg_httpreq.h"
#include "sg_httpsrv.h"

static void sg__httpsrv_oel(void *cls, const char *fmt, va_list ap) {
  struct sg_httpsrv *srv = cls;
  char err[SG_ERR_SIZE];
  vsnprintf(err, sizeof(err), fmt, ap);
  if (strcmp(err,
             _("Application reported internal error, closing connection.\n")) !=
      0)
    srv->err_cb(srv->cls, err);
}

static enum MHD_Result sg__httpsrv_ahc(void *cls, struct MHD_Connection *con,
                                       const char *url, const char *method,
                                       const char *version,
                                       const char *upld_data,
                                       size_t *upld_data_size, void **con_cls) {
  struct sg_httpsrv *srv = cls;
  struct sg_httpreq *req = *con_cls;
  const union MHD_ConnectionInfo *info;
  if (con) {
    info =
      MHD_get_connection_info(con, MHD_CONNECTION_INFO_SOCKET_CONTEXT, NULL);
    if (info && info->socket_context)
      return MHD_NO;
  }
  if (!req) {
    req = sg__httpreq_new(srv, con, version, method, url);
    if (!req)
      return MHD_NO;
    *con_cls = req;
    if (srv->auth_cb) {
      req->res->ret = srv->auth_cb(srv->cls, req->auth, req, req->res);
      if (!sg__httpauth_dispatch(req->auth))
        return req->res->ret;
    }
    return MHD_YES;
  }
  if (!req->auth->canceled) {
    if (sg__httpuplds_process(srv, req, con, upld_data, upld_data_size,
                              &req->res->ret))
      return req->res->ret;
    if (!req->isolated)
      srv->req_cb(srv->cls, req, req->res);
  }
  if (con) {
    info = MHD_get_connection_info(
      con, MHD_CONNECTION_INFO_CONNECTION_SUSPENDED, NULL);
  } else
    info = NULL;
  return info && info->suspended ? MHD_YES : sg__httpres_dispatch(req->res);
}

static void sg__httpsrv_rcc(void *cls, __SG_UNUSED struct MHD_Connection *con,
                            void **con_cls,
                            __SG_UNUSED enum MHD_RequestTerminationCode toe) {
  if (*con_cls) {
    sg__httpuplds_cleanup(cls, *con_cls);
    sg__httpreq_free(*con_cls);
  }
  *con_cls = NULL;
}

static void sg__httpsrv_ncc(void *cls, __SG_UNUSED struct MHD_Connection *con,
                            __SG_UNUSED void **socket_ctx,
                            enum MHD_ConnectionNotificationCode toe) {
  struct sg_httpsrv *srv = cls;
  const union MHD_ConnectionInfo *info;
  bool closed;
  if (!srv->cli_cb)
    return;
  info = MHD_get_connection_info(con, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
  switch (toe) {
    case MHD_CONNECTION_NOTIFY_STARTED:
      closed = false;
      srv->cli_cb(srv->cli_cls, info->client_addr, &closed);
      *((bool *) socket_ctx) = closed;
      break;
    case MHD_CONNECTION_NOTIFY_CLOSED:
      closed = true;
      srv->cli_cb(srv->cli_cls, info->client_addr, &closed);
      break;
    default:
      break;
  }
}

static void sg__httpsrv_addopt(struct MHD_OptionItem ops[14],
                               unsigned char *pos, enum MHD_OPTION opt,
                               intptr_t val, void *ptr) {
  ops[*pos].option = opt;
  ops[*pos].value = val;
  ops[*pos].ptr_value = ptr;
  (*pos)++;
}

static bool sg__httpsrv_listen2(struct sg_httpsrv *srv, const char *key,
                                const char *pwd, const char *cert,
                                const char *trust, const char *dhparams,
                                const char *priorities, const char *hostname,
                                uint16_t port, uint32_t backlog,
                                bool threaded) {
  struct MHD_OptionItem ops[14];
  struct sockaddr_in addr;
  struct sockaddr_in6 addr6;
  unsigned int flags;
  unsigned char pos = 0;
  int errnum;
  if (!srv || !srv->upld_cb || !srv->upld_write_cb || !srv->upld_save_cb ||
      !srv->upld_save_as_cb || !srv->uplds_dir || (srv->post_buf_size < 256)) {
    errno = EINVAL;
    return false;
  }
  flags = MHD_USE_ITC | MHD_USE_ERROR_LOG | MHD_ALLOW_SUSPEND_RESUME |
          (threaded ?
             MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION :
             MHD_USE_AUTO_INTERNAL_THREAD);
  sg__httpsrv_addopt(ops, &pos, MHD_OPTION_EXTERNAL_LOGGER,
                     (intptr_t) sg__httpsrv_oel, srv);
  if (hostname) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    errnum = inet_pton(AF_INET, hostname, &(addr.sin_addr));
    if (errnum == 1) {
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_SOCK_ADDR, 0,
                         (struct sockaddr *) (&addr));
    } else {
      memset(&addr6, 0, sizeof(addr6));
      addr6.sin6_family = AF_INET6;
      addr6.sin6_port = htons(port);
      errnum = inet_pton(AF_INET6, hostname, &(addr6.sin6_addr));
      if (errnum != 1) {
        sg__httpsrv_eprintf(srv, _("Invalid host name: %s.\n"), hostname);
        errno = EINVAL;
        return false;
      }
      flags |= MHD_USE_DUAL_STACK;
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_SOCK_ADDR, 0,
                         (struct sockaddr *) (&addr6));
    }
  } else {
    flags |= MHD_USE_DUAL_STACK;
  }
  sg__httpsrv_addopt(ops, &pos, MHD_OPTION_NOTIFY_COMPLETED,
                     (intptr_t) sg__httpsrv_rcc, srv);
  sg__httpsrv_addopt(ops, &pos, MHD_OPTION_NOTIFY_CONNECTION,
                     (intptr_t) sg__httpsrv_ncc, srv);
  if (srv->con_limit > 0)
    sg__httpsrv_addopt(ops, &pos, MHD_OPTION_CONNECTION_LIMIT, srv->con_limit,
                       NULL);
  if (srv->con_timeout > 0)
    sg__httpsrv_addopt(ops, &pos, MHD_OPTION_CONNECTION_TIMEOUT,
                       srv->con_timeout, NULL);
  if (srv->thr_pool_size > 0)
    sg__httpsrv_addopt(ops, &pos, MHD_OPTION_THREAD_POOL_SIZE,
                       srv->thr_pool_size, NULL);
  if (key && cert) {
    flags |= MHD_USE_TLS;
    sg__httpsrv_addopt(ops, &pos, MHD_OPTION_HTTPS_MEM_KEY, 0, (void *) key);
    if (pwd)
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_HTTPS_KEY_PASSWORD, 0,
                         (void *) pwd);
    sg__httpsrv_addopt(ops, &pos, MHD_OPTION_HTTPS_MEM_CERT, 0, (void *) cert);
    if (trust)
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_HTTPS_MEM_TRUST, 0,
                         (void *) trust);
    if (dhparams)
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_HTTPS_MEM_DHPARAMS, 0,
                         (void *) dhparams);
    if (priorities)
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_HTTPS_PRIORITIES, 0,
                         (void *) priorities);
    if (backlog > 0) {
      sg__httpsrv_addopt(ops, &pos, MHD_OPTION_LISTEN_BACKLOG_SIZE, backlog,
                         NULL);
    }
  }
  sg__httpsrv_addopt(ops, &pos, MHD_OPTION_END, 0, NULL);
  srv->handle = MHD_start_daemon(flags, port, NULL, NULL, sg__httpsrv_ahc, srv,
                                 MHD_OPTION_ARRAY, ops, MHD_OPTION_END);
  return srv->handle != NULL;
}

static bool sg__httpsrv_listen(struct sg_httpsrv *srv, const char *key,
                               const char *pwd, const char *cert,
                               const char *trust, const char *dhparams,
                               const char *priorities, uint16_t port,
                               bool threaded) {
  return sg__httpsrv_listen2(srv, key, pwd, cert, trust, dhparams, priorities,
                             NULL, port, 0, threaded);
}

void sg__httpsrv_eprintf(struct sg_httpsrv *srv, const char *fmt, ...) {
  char err[SG_ERR_SIZE];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(err, sizeof(err), fmt, ap);
  va_end(ap);
  srv->err_cb(srv->cls, err);
}

void sg__httpsrv_lock(struct sg_httpsrv *srv) {
  char err[SG_ERR_SIZE];
  int errnum = pthread_mutex_lock(&srv->mutex);
  if (errnum != 0)
    sg__httpsrv_eprintf(srv, _("Failed to lock mutex: %s.\n"),
                        sg_strerror(errnum, err, sizeof(err)));
}

void sg__httpsrv_unlock(struct sg_httpsrv *srv) {
  char err[SG_ERR_SIZE];
  int errnum = pthread_mutex_unlock(&srv->mutex);
  if (errnum != 0)
    sg__httpsrv_eprintf(srv, _("Failed to unlock mutex: %s.\n"),
                        sg_strerror(errnum, err, sizeof(err)));
}

struct sg_httpsrv *sg_httpsrv_new2(sg_httpauth_cb auth_cb, sg_httpreq_cb req_cb,
                                   sg_err_cb err_cb, void *cls) {
  struct sg_httpsrv *srv;
  int errnum;
  if (!req_cb || !err_cb) {
    errno = EINVAL;
    return NULL;
  }
  srv = sg_alloc(sizeof(struct sg_httpsrv));
  if (!srv)
    return NULL;
  srv->uplds_dir = sg_tmpdir();
  if (!srv->uplds_dir) {
    sg_free(srv);
    return NULL;
  }
  errnum = pthread_mutex_init(&srv->mutex, NULL);
  if (errnum != 0) {
    sg_free(srv->uplds_dir);
    sg_free(srv);
    errno = errnum;
    return NULL;
  }
  srv->auth_cb = auth_cb;
  srv->req_cb = req_cb;
  srv->err_cb = err_cb;
  srv->cls = cls;
  srv->upld_cb = sg__httpupld_cb;
  srv->upld_cls = srv;
  srv->upld_write_cb = sg__httpupld_write_cb;
  srv->upld_free_cb = sg__httpupld_free_cb;
  srv->upld_save_cb = sg__httpupld_save_cb;
  srv->upld_save_as_cb = sg__httpupld_save_as_cb;
#ifdef __arm__
  srv->post_buf_size = 1024; /* ~1 Kb */
  srv->payld_limit = 1048576; /* ~1 MB */
  srv->uplds_limit = 16777216; /* ~16 MB */
#else /* __arm__ */
  srv->post_buf_size = 4096; /* ~4 kB */
  srv->payld_limit = 4194304; /* ~4 MB */
  srv->uplds_limit = 67108864; /* ~64 MB */
#endif /* __arm__ */
  return srv;
}

struct sg_httpsrv *sg_httpsrv_new(sg_httpreq_cb cb, void *cls) {
  return sg_httpsrv_new2(NULL, cb, sg__err_cb, cls);
}

void sg_httpsrv_free(struct sg_httpsrv *srv) {
  const union MHD_ConnectionInfo *info;
  struct sg__httpreq_isolated *isolated, *tmp;
  char err[SG_ERR_SIZE];
  int errnum;
  if (!srv)
    return;
  sg__httpsrv_lock(srv);
  LL_FOREACH_SAFE(srv->isolated_list, isolated, tmp) {
    sg__httpsrv_unlock(srv);
    errnum = pthread_join(isolated->thread, NULL);
    if (errnum != 0)
      sg__httpsrv_eprintf(srv, _("Failed to join thread %p: %s.\n"),
                          (void *) &isolated->thread,
                          sg_strerror(errnum, err, sizeof(err)));
    sg__httpsrv_lock(srv);
  }
  sg__httpsrv_unlock(srv);
  sg__httpsrv_lock(srv);
  LL_FOREACH_SAFE(srv->isolated_list, isolated, tmp) {
    info = MHD_get_connection_info(
      isolated->handle->con, MHD_CONNECTION_INFO_CONNECTION_SUSPENDED, NULL);
    if (info && info->suspended)
      MHD_resume_connection(isolated->handle->con);
    LL_DELETE(srv->isolated_list, isolated);
    sg_free(isolated);
  }
  sg__httpsrv_unlock(srv);
  sg_httpsrv_shutdown(srv);
  sg_free(srv->uplds_dir);
  pthread_mutex_destroy(&srv->mutex);
  sg_free(srv);
}

#ifdef SG_HTTPS_SUPPORT

bool sg_httpsrv_tls_listen4(struct sg_httpsrv *srv, const char *key,
                            const char *pwd, const char *cert,
                            const char *trust, const char *dhparams,
                            const char *priorities, const char *hostname,
                            uint16_t port, uint32_t backlog, bool threaded) {
  if (key && cert)
    return sg__httpsrv_listen2(srv, key, pwd, cert, trust, dhparams, priorities,
                               hostname, port, backlog, threaded);
  errno = EINVAL;
  return false;
}

bool sg_httpsrv_tls_listen3(struct sg_httpsrv *srv, const char *key,
                            const char *pwd, const char *cert,
                            const char *trust, const char *dhparams,
                            const char *priorities, uint16_t port,
                            bool threaded) {
  if (key && cert)
    return sg__httpsrv_listen(srv, key, pwd, cert, trust, dhparams, priorities,
                              port, threaded);
  errno = EINVAL;
  return false;
}

bool sg_httpsrv_tls_listen2(struct sg_httpsrv *srv, const char *key,
                            const char *pwd, const char *cert,
                            const char *trust, const char *dhparams,
                            uint16_t port, bool threaded) {
  if (key && cert)
    return sg__httpsrv_listen(srv, key, pwd, cert, trust, dhparams, NULL, port,
                              threaded);
  errno = EINVAL;
  return false;
}

bool sg_httpsrv_tls_listen(struct sg_httpsrv *srv, const char *key,
                           const char *cert, uint16_t port, bool threaded) {
  if (key && cert)
    return sg__httpsrv_listen(srv, key, NULL, cert, NULL, NULL, NULL, port,
                              threaded);
  errno = EINVAL;
  return false;
}

#endif /* SG_HTTPS_SUPPORT */

bool sg_httpsrv_listen2(struct sg_httpsrv *srv, const char *hostname,
                        uint16_t port, uint32_t backlog, bool threaded) {
  return sg__httpsrv_listen2(srv, NULL, NULL, NULL, NULL, NULL, NULL, hostname,
                             port, backlog, threaded);
}

bool sg_httpsrv_listen(struct sg_httpsrv *srv, uint16_t port, bool threaded) {
  return sg__httpsrv_listen(srv, NULL, NULL, NULL, NULL, NULL, NULL, port,
                            threaded);
}

int sg_httpsrv_shutdown(struct sg_httpsrv *srv) {
  if (!srv)
    return EINVAL;
  if (!srv->handle)
    return EALREADY;
  MHD_stop_daemon(srv->handle);
  srv->handle = NULL;
  return 0;
}

uint16_t sg_httpsrv_port(struct sg_httpsrv *srv) {
  if (srv)
    return srv->handle ?
             MHD_get_daemon_info(srv->handle, MHD_DAEMON_INFO_BIND_PORT)->port :
             (uint16_t) 0;
  errno = EINVAL;
  return 0;
}

bool sg_httpsrv_is_threaded(struct sg_httpsrv *srv) {
  if (srv)
    return srv->handle &&
           (MHD_get_daemon_info(srv->handle, MHD_DAEMON_INFO_FLAGS)->flags &
            MHD_USE_THREAD_PER_CONNECTION);
  errno = EINVAL;
  return false;
}

int sg_httpsrv_set_cli_cb(struct sg_httpsrv *srv, sg_httpsrv_cli_cb cb,
                          void *cls) {
  if (!srv || !cb)
    return EINVAL;
  srv->cli_cb = cb;
  srv->cli_cls = cls;
  return 0;
}

int sg_httpsrv_set_upld_cbs(struct sg_httpsrv *srv, sg_httpupld_cb cb,
                            void *cls, sg_write_cb write_cb, sg_free_cb free_cb,
                            sg_save_cb save_cb, sg_save_as_cb save_as_cb) {
  if (!srv || !cb || !write_cb || !save_cb || !save_as_cb)
    return EINVAL;
  srv->upld_cb = cb;
  srv->upld_write_cb = write_cb;
  srv->upld_free_cb = free_cb;
  srv->upld_save_cb = save_cb;
  srv->upld_save_as_cb = save_as_cb;
  srv->upld_cls = cls;
  return 0;
}

int sg_httpsrv_set_upld_dir(struct sg_httpsrv *srv, const char *dir) {
  if (!srv || !dir)
    return EINVAL;
  sg_free(srv->uplds_dir);
  srv->uplds_dir = strdup(dir);
  return 0;
}

const char *sg_httpsrv_upld_dir(struct sg_httpsrv *srv) {
  if (srv)
    return srv->uplds_dir;
  errno = EINVAL;
  return NULL;
}

int sg_httpsrv_set_post_buf_size(struct sg_httpsrv *srv, size_t size) {
  if (!srv || (size < 256))
    return EINVAL;
  srv->post_buf_size = size;
  return 0;
}

size_t sg_httpsrv_post_buf_size(struct sg_httpsrv *srv) {
  if (srv)
    return srv->post_buf_size;
  errno = EINVAL;
  return 0;
}

int sg_httpsrv_set_payld_limit(struct sg_httpsrv *srv, size_t limit) {
  if (!srv)
    return EINVAL;
  srv->payld_limit = limit;
  return 0;
}

size_t sg_httpsrv_payld_limit(struct sg_httpsrv *srv) {
  if (srv)
    return srv->payld_limit;
  errno = EINVAL;
  return 0;
}

int sg_httpsrv_set_uplds_limit(struct sg_httpsrv *srv, uint64_t limit) {
  if (!srv)
    return EINVAL;
  srv->uplds_limit = limit;
  return 0;
}

uint64_t sg_httpsrv_uplds_limit(struct sg_httpsrv *srv) {
  if (srv)
    return srv->uplds_limit;
  errno = EINVAL;
  return 0;
}

int sg_httpsrv_set_thr_pool_size(struct sg_httpsrv *srv, unsigned int size) {
  if (!srv)
    return EINVAL;
  srv->thr_pool_size = size;
  return 0;
}

unsigned int sg_httpsrv_thr_pool_size(struct sg_httpsrv *srv) {
  if (srv)
    return srv->thr_pool_size;
  errno = EINVAL;
  return 0;
}

int sg_httpsrv_set_con_timeout(struct sg_httpsrv *srv, unsigned int timeout) {
  if (!srv)
    return EINVAL;
  srv->con_timeout = timeout;
  return 0;
}

unsigned int sg_httpsrv_con_timeout(struct sg_httpsrv *srv) {
  if (srv)
    return srv->con_timeout;
  errno = EINVAL;
  return 0;
}

int sg_httpsrv_set_con_limit(struct sg_httpsrv *srv, unsigned int limit) {
  if (!srv)
    return EINVAL;
  srv->con_limit = limit;
  return 0;
}

unsigned int sg_httpsrv_con_limit(struct sg_httpsrv *srv) {
  if (srv)
    return srv->con_limit;
  errno = EINVAL;
  return 0;
}

void *sg_httpsrv_handle(struct sg_httpsrv *srv) {
  if (srv)
    return srv->handle;
  errno = EINVAL;
  return 0;
}
