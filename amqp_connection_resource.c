/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2007 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Alexandre Kalendarev akalend@mail.ru Copyright (c) 2009-2010 |
  | Lead:                                                                |
  | - Pieter de Zwart                                                    |
  | Maintainers:                                                         |
  | - Brad Rodriguez                                                     |
  | - Jonathan Tansavatdi                                                |
  +----------------------------------------------------------------------+
*/

/* $Id: amqp_connection.c 327551 2012-09-09 03:49:34Z pdezwart $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"

#ifdef PHP_WIN32
# include "win32/php_stdint.h"
# include "win32/signal.h"
#else
# include <signal.h>
# include <stdint.h>
#endif
#include <amqp.h>
#include <amqp_framing.h>
#include <amqp_tcp_socket.h>

#ifdef PHP_WIN32
# include "win32/unistd.h"
#else
# include <unistd.h>
#endif

#include "php_amqp.h"
#include "amqp_connection_resource.h"

#ifndef E_DEPRECATED
#define E_DEPRECATED E_WARNING
#endif

static void connection_resource_destructor(amqp_connection_resource *resource, int persistent TSRMLS_DC);


/* Figure out what's going on connection and handle protocol exceptions, if any */

int php_amqp_connection_resource_error(amqp_rpc_reply_t x, char **pstr, amqp_connection_resource *resource, amqp_channel_t channel_id)
{
	assert (resource != NULL);

	switch (x.reply_type) {
		case AMQP_RESPONSE_NORMAL:
			return PHP_AMQP_RESOURCE_RESPONSE_OK;

		case AMQP_RESPONSE_NONE:
			spprintf(pstr, 0, "Missing RPC reply type.");
			return PHP_AMQP_RESOURCE_RESPONSE_ERROR;

		case AMQP_RESPONSE_LIBRARY_EXCEPTION:
			spprintf(pstr, 0, "Library error: %s", amqp_error_string2(x.library_error));
			return PHP_AMQP_RESOURCE_RESPONSE_ERROR;

		case AMQP_RESPONSE_SERVER_EXCEPTION:
			switch (x.reply.id) {
				case AMQP_CONNECTION_CLOSE_METHOD: {
					amqp_connection_close_t *m = (amqp_connection_close_t *)x.reply.decoded;
					spprintf(pstr, 0, "Server connection error: %d, message: %.*s",
						m->reply_code,
						(int) m->reply_text.len,
						(char *)m->reply_text.bytes);

					/*
					 *    - If r.reply.id == AMQP_CONNECTION_CLOSE_METHOD a connection exception
					 *      occurred, cast r.reply.decoded to amqp_connection_close_t* to see
					 *      details of the exception. The client amqp_send_method() a
					 *      amqp_connection_close_ok_t and disconnect from the broker.
					 */

					amqp_connection_close_ok_t *decoded = (amqp_connection_close_ok_t *) NULL;

					amqp_send_method(
						resource->connection_state,
						0, /* NOTE: 0-channel is reserved for things like this */
						AMQP_CONNECTION_CLOSE_OK_METHOD,
						&decoded
					);

					/* Prevent finishing AMQP connection in connection resource destructor */
					resource->is_connected = '\0';

					return PHP_AMQP_RESOURCE_RESPONSE_ERROR_CONNECTION_CLOSED;
				}
				case AMQP_CHANNEL_CLOSE_METHOD: {
					assert(channel_id > 0 && channel_id <= PHP_AMQP_MAX_CHANNELS);

					amqp_channel_close_t *m = (amqp_channel_close_t *) x.reply.decoded;

					spprintf(pstr, 0, "Server channel error: %d, message: %.*s",
						m->reply_code,
						(int)m->reply_text.len,
						(char *)m->reply_text.bytes);

					/*
					 *    - If r.reply.id == AMQP_CHANNEL_CLOSE_METHOD a channel exception
					 *      occurred, cast r.reply.decoded to amqp_channel_close_t* to see details
					 *      of the exception. The client should amqp_send_method() a
					 *      amqp_channel_close_ok_t. The channel must be re-opened before it
					 *      can be used again. Any resources associated with the channel
					 *      (auto-delete exchanges, auto-delete queues, consumers) are invalid
					 *      and must be recreated before attempting to use them again.
					 */

					amqp_channel_close_ok_t *decoded = (amqp_channel_close_ok_t *) NULL;

					amqp_send_method(
						resource->connection_state,
						channel_id,
						AMQP_CHANNEL_CLOSE_OK_METHOD,
						&decoded
					);

					return PHP_AMQP_RESOURCE_RESPONSE_ERROR_CHANNEL_CLOSED;
				}
			}
		/* Default for the above switch should be handled by the below default. */
		default:
			spprintf(pstr, 0, "Unknown server error, method id 0x%08X",	x.reply.id);
			return PHP_AMQP_RESOURCE_RESPONSE_ERROR;
	}

	/* Should not never get here*/
}


/* Socket-related functions */
int php_amqp_set_resource_read_timeout(amqp_connection_resource *resource, double timeout TSRMLS_DC)
{
#ifdef PHP_WIN32
	DWORD read_timeout;
	/*
	In Windows, setsockopt with SO_RCVTIMEO sets actual timeout
	to a value that's 500ms greater than specified value.
	Also, it's not possible to set timeout to any value below 500ms.
	Zero timeout works like it should, however.
	*/
	if (timeout == 0.) {
		read_timeout = 0;
	} else {
		read_timeout = (int) (max(connection->read_timeout * 1.e+3 - .5e+3, 1.));
	}
#else
	struct timeval read_timeout;
	read_timeout.tv_sec = (int) floor(timeout);
	read_timeout.tv_usec = (int) ((timeout - floor(timeout)) * 1.e+6);
#endif

	if (0 != setsockopt(amqp_get_sockfd(resource->connection_state), SOL_SOCKET, SO_RCVTIMEO, (char *)&read_timeout, sizeof(read_timeout))) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Socket error: cannot setsockopt SO_RCVTIMEO", 0 TSRMLS_CC);
		return 0;
	}

	return 1;
}

int php_amqp_set_resource_write_timeout(amqp_connection_resource *resource, double timeout TSRMLS_DC)
{
#ifdef PHP_WIN32
	DWORD write_timeout;

	if (timeout == 0.) {
		write_timeout = 0;
	} else {
		write_timeout = (int) (max(timeout * 1.e+3 - .5e+3, 1.));
	}
#else
	struct timeval write_timeout;
	write_timeout.tv_sec = (int) floor(timeout);
	write_timeout.tv_usec = (int) ((timeout - floor(timeout)) * 1.e+6);
#endif

	if (0 != setsockopt(amqp_get_sockfd(resource->connection_state), SOL_SOCKET, SO_SNDTIMEO, (char *)&write_timeout, sizeof(write_timeout))) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Socket error: cannot setsockopt SO_SNDTIMEO", 0 TSRMLS_CC);
		return 0;
	}

	return 1;
}


/* Channel-related functions */

amqp_channel_t php_amqp_connection_resource_get_available_channel_id(amqp_connection_resource *resource)
{
	assert(resource != NULL);
	assert(resource->slots != NULL);

	/* Check if there are any open slots */
	if (resource->used_slots >= PHP_AMQP_MAX_CHANNELS + 1) {
    	return 0;
    }

	amqp_channel_t slot;

	for (slot = 1; slot < PHP_AMQP_MAX_CHANNELS + 1; slot++) {
		if (resource->slots[slot] == 0) {
			return slot;
		}
	}

	return 0;
}

int php_amqp_connection_resource_register_channel(amqp_connection_resource *resource, amqp_channel_object *channel, amqp_channel_t channel_id)
{
	assert(resource != NULL);
	assert(resource->slots != NULL);
	assert(channel_id > 0 && channel_id <= PHP_AMQP_MAX_CHANNELS);

	if (resource->slots[channel_id] != 0) {
		return FAILURE;
	}

	resource->slots[channel_id] = channel;
	resource->used_slots++;

	return SUCCESS;
}

int php_amqp_connection_resource_unregister_channel(amqp_connection_resource *resource, amqp_channel_t channel_id)
{
	assert(resource != NULL);
	assert(resource->slots != NULL);
	assert(channel_id > 0 && channel_id <= PHP_AMQP_MAX_CHANNELS);

	if (resource->slots[channel_id] != 0) {
		resource->slots[channel_id] = 0;
		resource->used_slots++;
	}

	return SUCCESS;
}


/* Creating and destroying resource */

amqp_connection_resource *connection_resource_constructor(amqp_connection_object *connection, zend_bool persistent TSRMLS_DC)
{
	struct timeval tv = {0};
	struct timeval *tv_ptr = &tv;

	amqp_connection_resource *resource;

	/* Allocate space for the connection resource */
	resource = (amqp_connection_resource *)pemalloc(sizeof(amqp_connection_resource), persistent);
	memset(resource, 0, sizeof(amqp_connection_resource));

	/* Allocate space for the channel slots in the ring buffer */
	resource->slots = (amqp_channel_object **)pecalloc(PHP_AMQP_MAX_CHANNELS + 1, sizeof(amqp_channel_object*), persistent);
	memset(resource->slots, 0, sizeof(amqp_channel_object*));

	/* Initialize all the data */
	resource->is_connected  = 0;
	resource->used_slots    = 0;
	resource->resource_id   = 0;

	/* Create the connection */
	resource->connection_state = amqp_new_connection();

	/* Create socket object */
	resource->socket = amqp_tcp_socket_new(resource->connection_state);

	if (connection->connect_timeout > 0) {
		tv.tv_sec = (long int) connection->connect_timeout;
		tv.tv_usec = (long int) ((connection->connect_timeout - tv.tv_sec) * 1000000);
	} else {
		tv_ptr = NULL;
	}

	/* Try to connect and verify that no error occurred */
	if (amqp_socket_open_noblock(resource->socket, connection->host, connection->port, tv_ptr)) {

		connection_resource_destructor(resource, persistent TSRMLS_CC);

		zend_throw_exception(amqp_connection_exception_class_entry, "Socket error: could not connect to host.", 0 TSRMLS_CC);
		return NULL;
	}

	if (!php_amqp_set_resource_read_timeout(resource, connection->read_timeout TSRMLS_CC)) {
		connection_resource_destructor(resource, persistent TSRMLS_CC);
		return NULL;
	}

	if (!php_amqp_set_resource_write_timeout(resource, connection->write_timeout TSRMLS_CC)) {
		connection_resource_destructor(resource, persistent TSRMLS_CC);
		return NULL;
	}

	/* Assume we established connection here (but it is not true, real handshake goes during login) */
	resource->is_connected = '\1';

    amqp_table_entry_t client_properties_entries[5];
    amqp_table_t       client_properties_table;

	client_properties_entries[0].key 			   = amqp_cstring_bytes("client type");
	client_properties_entries[0].value.kind        = AMQP_FIELD_KIND_UTF8;
	client_properties_entries[0].value.value.bytes = amqp_cstring_bytes("php-amqp extension");

	client_properties_entries[1].key 			   = amqp_cstring_bytes("client version");
	client_properties_entries[1].value.kind        = AMQP_FIELD_KIND_UTF8;
	client_properties_entries[1].value.value.bytes = amqp_cstring_bytes("PHP_AMQP_VERSION");

	client_properties_entries[2].key 			   = amqp_cstring_bytes("client version");
	client_properties_entries[2].value.kind        = AMQP_FIELD_KIND_UTF8;
	client_properties_entries[2].value.value.bytes = amqp_cstring_bytes(PHP_AMQP_VERSION);

	client_properties_entries[3].key 			   = amqp_cstring_bytes("client revision");
	client_properties_entries[3].value.kind        = AMQP_FIELD_KIND_UTF8;
	client_properties_entries[3].value.value.bytes = amqp_cstring_bytes(PHP_AMQP_REVISION);

	client_properties_entries[4].key 			   = amqp_cstring_bytes("client connection");
	client_properties_entries[4].value.kind        = AMQP_FIELD_KIND_UTF8;
	client_properties_entries[4].value.value.bytes = amqp_cstring_bytes(persistent ? "persistent" : "transient");

    client_properties_table.entries = client_properties_entries;
    client_properties_table.num_entries = sizeof(client_properties_entries) / sizeof(amqp_table_entry_t);

	amqp_rpc_reply_t res = amqp_login_with_properties(
		resource->connection_state,
		connection->vhost,
		PHP_AMQP_PROTOCOL_MAX_CHANNELS,
		AMQP_DEFAULT_FRAME_SIZE,
		PHP_AMQP_HEARTBEAT,
		&client_properties_table,
		AMQP_SASL_METHOD_PLAIN,
		connection->login,
		connection->password
	);

	if (res.reply_type != AMQP_RESPONSE_NORMAL) {
		char str[256];
    	char ** pstr = (char **) &str;
		php_amqp_connection_resource_error(res, pstr, resource, 0 TSRMLS_CC);

		connection_resource_destructor(resource, persistent TSRMLS_CC);

		strcat(*pstr, " - Potential login failure.");
		zend_throw_exception(amqp_connection_exception_class_entry, *pstr, 0 TSRMLS_CC);
		return NULL;
	}

	return resource;
}

ZEND_RSRC_DTOR_FUNC(amqp_connection_resource_dtor_persistent)
{
	amqp_connection_resource *resource = (amqp_connection_resource *)rsrc->ptr;

	connection_resource_destructor(resource, 1 TSRMLS_CC);
	php_printf("Persistent resource dtor called\n");
}

ZEND_RSRC_DTOR_FUNC(amqp_connection_resource_dtor)
{
	amqp_connection_resource *resource = (amqp_connection_resource *)rsrc->ptr;

	connection_resource_destructor(resource, 0 TSRMLS_CC);
	php_printf("Non-persistent resource dtor called\n");
}

static void connection_resource_destructor(amqp_connection_resource *resource, int persistent TSRMLS_DC)
{
	assert(resource != NULL);

	zend_rsrc_list_entry *le;
#ifndef PHP_WIN32
	void * old_handler;

	/*
	If we are trying to close the connection and the connection already closed, it will throw
	SIGPIPE, which is fine, so ignore all SIGPIPES
	*/

	/* Start ignoring SIGPIPE */
	old_handler = signal(SIGPIPE, SIG_IGN);
#endif

	/* connection may be closed in case of previous failure */
	if (resource->is_connected) {
		amqp_connection_close(resource->connection_state, AMQP_REPLY_SUCCESS);
	}

	amqp_destroy_connection(resource->connection_state);

#ifndef PHP_WIN32
	/* End ignoring of SIGPIPEs */
	signal(SIGPIPE, old_handler);
#endif

	if (resource->resource_key_len) {
		pefree(resource->resource_key, persistent);
	}

	pefree(resource->slots, persistent);
	pefree(resource, persistent);
}


