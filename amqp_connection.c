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

#ifndef E_DEPRECATED
#define E_DEPRECATED E_WARNING
#endif

int php_amqp_set_read_timeout(amqp_connection_object *connection TSRMLS_DC);
int php_amqp_set_write_timeout(amqp_connection_object *connection TSRMLS_DC);

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3
zend_object_handlers amqp_connection_object_handlers;
HashTable *amqp_connection_object_get_debug_info(zval *object, int *is_temp TSRMLS_DC) {
	zval *value;
	HashTable *debug_info;
	amqp_connection_object *connection;

	/* Let zend clean up for us: */
	*is_temp = 1;

	/* Get the envelope object from which to read */
	connection = (amqp_connection_object *)zend_object_store_get_object(object TSRMLS_CC);

	/* Keep the first number matching the number of entries in this table*/
	ALLOC_HASHTABLE(debug_info);
	ZEND_INIT_SYMTABLE_EX(debug_info, 6 + 1, 0);

	/* Start adding values */
	MAKE_STD_ZVAL(value);
	ZVAL_STRINGL(value, connection->login, strlen(connection->login), 1);
	zend_hash_add(debug_info, "login", sizeof("login"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_STRINGL(value, connection->password, strlen(connection->password), 1);
	zend_hash_add(debug_info, "password", sizeof("password"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_STRINGL(value, connection->host, strlen(connection->host), 1);
	zend_hash_add(debug_info, "host", sizeof("host"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_STRINGL(value, connection->vhost, strlen(connection->vhost), 1);
	zend_hash_add(debug_info, "vhost", sizeof("vhost"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_LONG(value, connection->port);
	zend_hash_add(debug_info, "port", sizeof("port"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_DOUBLE(value, connection->read_timeout);
	zend_hash_add(debug_info, "read_timeout", sizeof("read_timeout"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_DOUBLE(value, connection->write_timeout);
	zend_hash_add(debug_info, "write_timeout", sizeof("write_timeout"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_DOUBLE(value, connection->connect_timeout);
	zend_hash_add(debug_info, "connect_timeout", sizeof("connect_timeout"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);
	ZVAL_BOOL(value, connection->is_connected);
	zend_hash_add(debug_info, "is_connected", sizeof("is_connected"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);

	if (connection && connection->connection_resource) {
		Z_ADDREF_P(value);
		ZVAL_RESOURCE(value, connection->connection_resource->resource_id);
	} else {
		ZVAL_NULL(value);
	}

	zend_hash_add(debug_info, "connection_resource", sizeof("connection_resource"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);

	if (connection && connection->connection_resource) {
		ZVAL_BOOL(value, connection->connection_resource->is_persistent);
	} else {
		ZVAL_NULL(value);
	}

	zend_hash_add(debug_info, "is_persistent", sizeof("is_persistent"), &value, sizeof(zval *), NULL);

	MAKE_STD_ZVAL(value);

	if (connection && connection->connection_resource) {
		ZVAL_LONG(value, connection->connection_resource->last_channel_id);
	} else {
		ZVAL_NULL(value);
	}

	zend_hash_add(debug_info, "last_channel_id", sizeof("last_channel_id"), &value, sizeof(zval *), NULL);


//	amqp_channel_object **channel;
//	MAKE_STD_ZVAL(value);
//	array_init(value);
//
//	for (zend_hash_internal_pointer_reset(connection->channels_hashtable);
//		 zend_hash_get_current_data(connection->channels_hashtable, (void **) &channel) == SUCCESS;
//		 zend_hash_move_forward(connection->channels_hashtable)
//	) {
//		add_next_index_long(value, (*channel)->channel_id);
//	}
//
//	Z_ADDREF_P(value);
//	zend_hash_add(debug_info, "opened_channels", sizeof("opened_channels"), &value, sizeof(zval *), NULL);

	/* Start adding values */
	return debug_info;
}
#endif


/* 	php_amqp_disconnect
	handles disconnecting from amqp
	called by disconnect(), reconnect(), and d_tor
 */
void php_amqp_disconnect(amqp_connection_object *connection TSRMLS_DC)
{
	amqp_channel_object **channel;

//	printf(" - php_amqp_disconnect\n");

	/* Pull the connection resource out for easy access */
	amqp_connection_resource *resource = connection->connection_resource;

	/* Iterate over hashtable and close all channels, if any. */

	/* NOTE: when we have persistent connection we do not move channels between php requests
	 *       due to current php-amqp extension limitation in AMQPChannel where __construct == channel.open AMQP method call
	 *       and __destruct = channel.close AMQP method call
	 */

	for (zend_hash_internal_pointer_reset(connection->channels_hashtable);
		 zend_hash_get_current_data(connection->channels_hashtable, (void **) &channel) == SUCCESS;
		 zend_hash_move_forward(connection->channels_hashtable)
	) {
		php_amqp_close_channel(*channel TSRMLS_CC);
	}

	/* If it's persistent connection do not destroy connection resource (this keep connection alive) */
	if (resource && resource->is_persistent) {
		return;
	}

	if (resource) {
//		printf("Delete resource #%d\n", resource->resource_id);

		zend_list_delete(resource->resource_id);

//		printf("Resource deleted\n");

		connection->connection_resource = NULL;
	}

	connection->is_connected = '\0';

	return;
}

int php_amqp_start_connection(amqp_connection_object *connection, int persistent TSRMLS_DC)
{
	char str[256];
	char ** pstr = (char **) &str;

	amqp_rpc_reply_t x;
	struct timeval tv = {0};
	struct timeval *tv_ptr = &tv;

//	printf("  + creating new resource!\n");

	/* Allocate space for the connection resource */
	connection->connection_resource = (amqp_connection_resource *)pemalloc(sizeof(amqp_connection_resource), persistent);
	memset(connection->connection_resource, 0, sizeof(amqp_connection_resource));

	connection->connection_resource->last_channel_id = 0;

	connection->connection_resource->resource_id = ZEND_REGISTER_RESOURCE(NULL, connection->connection_resource, persistent ? le_amqp_connection_resource_persistent : le_amqp_connection_resource);

	/* Mark this as non persistent resource */
	connection->connection_resource->is_persistent = persistent;

	/* Create the connection */
	connection->connection_resource->connection_state = amqp_new_connection();

	/* Create socket object */
	connection->connection_resource->socket = amqp_tcp_socket_new(connection->connection_resource->connection_state);

//	printf("  + going to open connection\n");
	if (connection->connect_timeout > 0) {
		tv.tv_sec = (long int) connection->connect_timeout;
		tv.tv_usec = (long int) ((connection->connect_timeout - tv.tv_sec) * 1000000);
	} else {
		tv_ptr = NULL;
	}

	/* Try to connect and verify that no error occurred */
	if (amqp_socket_open_noblock(connection->connection_resource->socket, connection->host, connection->port, tv_ptr)) {

//		printf("  + going to open connection - failed\n");

		php_amqp_disconnect(connection TSRMLS_CC);

		zend_throw_exception(amqp_connection_exception_class_entry, "Socket error: could not connect to host.", 0 TSRMLS_CC);
		return 0;
	}

	php_amqp_set_read_timeout(connection TSRMLS_CC);
	php_amqp_set_write_timeout(connection TSRMLS_CC);

	x = amqp_login(
		connection->connection_resource->connection_state,
		connection->vhost,
		AMQP_DEFAULT_MAX_CHANNELS,
		AMQP_DEFAULT_FRAME_SIZE,
		AMQP_HEARTBEAT,
		AMQP_SASL_METHOD_PLAIN,
		connection->login,
		connection->password
	);

	if (x.reply_type != AMQP_RESPONSE_NORMAL) {
		amqp_error(x, pstr, connection, NULL TSRMLS_CC);

		strcat(*pstr, " - Potential login failure.");
		zend_throw_exception(amqp_connection_exception_class_entry, *pstr, 0 TSRMLS_CC);
		return 0;
	}

	connection->is_connected = '\1';

	return 1;
}

/**
 * 	php_amqp_connect
 *	handles connecting to amqp
 *	called by connect() and reconnect()
 */
int php_amqp_connect(amqp_connection_object *connection, int persistent TSRMLS_DC)
{
	char *key;
	int key_len;
	zend_rsrc_list_entry *le, new_le;

	int result;

//	printf(" + php_amqp_connect\n");

	/* Clean up old memory allocations which are now invalid (new connection) */
	assert(connection->connection_resource == NULL);
	assert(!connection->is_connected);

	if (persistent) {
//		printf("  + wannabe persistent, searching\n");
		/* Look for an established resource */
    	key_len = spprintf(&key, 0, "amqp_conn_res_%s_%d_%s_%s", connection->host, connection->port, connection->vhost, connection->login);

		if (zend_hash_find(&EG(persistent_list), key, key_len + 1, (void **)&le) == SUCCESS) {
			/* An entry for this connection resource already exists */
			/* Stash the connection resource in the connection */
			connection->connection_resource = le->ptr;

			/* TODO: to check whether connection (raw socket) stay alive we have to do some socket operation */

			/* Set connection status to connected */
			connection->is_connected = '\1';
//			printf("  + wannabe persistent - found!\n");
		} else {
			result = php_amqp_start_connection(connection, persistent TSRMLS_CC);

			if (result) {
				connection->connection_resource->resource_key     = pestrndup(key, key_len, persistent);
				connection->connection_resource->resource_key_len = key_len;

				/* Store a reference in the persistence list */
				new_le.ptr = connection->connection_resource;
				new_le.type = persistent ? le_amqp_connection_resource_persistent : le_amqp_connection_resource;
				zend_hash_add(&EG(persistent_list), key, key_len + 1, &new_le, sizeof(zend_rsrc_list_entry), NULL);

//				printf("  + resource stored!\n");
			}

			efree(key);

			return result;
		}
	} else {
		return php_amqp_start_connection(connection, persistent TSRMLS_CC);
	}

	return 1;
}

int php_amqp_set_read_timeout(amqp_connection_object *connection TSRMLS_DC)
{
#ifdef PHP_WIN32
	DWORD read_timeout;
	/*
	In Windows, setsockopt with SO_RCVTIMEO sets actual timeout
	to a value that's 500ms greater than specified value.
	Also, it's not possible to set timeout to any value below 500ms.
	Zero timeout works like it should, however.
	*/
	if (connection->read_timeout == 0.) {
		read_timeout = 0;
	} else {
		read_timeout = (int) (max(connection->read_timeout * 1.e+3 - .5e+3, 1.));
	}
#else
	struct timeval read_timeout;
	read_timeout.tv_sec = (int) floor(connection->read_timeout);
	read_timeout.tv_usec = (int) ((connection->read_timeout - floor(connection->read_timeout)) * 1.e+6);
#endif

	if (0 != setsockopt(amqp_get_sockfd(connection->connection_resource->connection_state), SOL_SOCKET, SO_RCVTIMEO, (char *)&read_timeout, sizeof(read_timeout))) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Socket error: cannot setsockopt SO_RCVTIMEO", 0 TSRMLS_CC);
		return 0;
	}

	return 1;
}

int php_amqp_set_write_timeout(amqp_connection_object *connection TSRMLS_DC)
{
#ifdef PHP_WIN32
	DWORD write_timeout;

	if (connection->write_timeout == 0.) {
		write_timeout = 0;
	} else {
		write_timeout = (int) (max(connection->write_timeout * 1.e+3 - .5e+3, 1.));
	}
#else
	struct timeval write_timeout;
	write_timeout.tv_sec = (int) floor(connection->write_timeout);
	write_timeout.tv_usec = (int) ((connection->write_timeout - floor(connection->write_timeout)) * 1.e+6);
#endif

	if (0 != setsockopt(amqp_get_sockfd(connection->connection_resource->connection_state), SOL_SOCKET, SO_SNDTIMEO, (char *)&write_timeout, sizeof(write_timeout))) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Socket error: cannot setsockopt SO_SNDTIMEO", 0 TSRMLS_CC);
		return 0;
	}

	return 1;
}

int unsigned get_next_available_channel_id(amqp_connection_object *connection, amqp_channel_object *channel)
{
	/* Pull out the ring buffer for ease of use */
	amqp_connection_resource *resource = connection->connection_resource;

	/* Check if there are any open slots */
	if (resource->last_channel_id > PHP_AMQP_MAX_CHANNELS - 1) {
		return 0;
	}

	return ++resource->last_channel_id;
}

void amqp_connection_dtor(void *object TSRMLS_DC)
{
//	printf("connectoin class dtor called\n");

    int slot;
	amqp_connection_object *connection = (amqp_connection_object*)object;

	php_amqp_disconnect(connection TSRMLS_CC);

	zend_hash_clean(connection->channels_hashtable);

	zend_hash_destroy(connection->channels_hashtable);
	FREE_HASHTABLE(connection->channels_hashtable);

	/* Clean up all the strings */
	if (connection->host) {
		efree(connection->host);
	}

	if (connection->vhost) {
		efree(connection->vhost);
	}

	if (connection->login) {
		efree(connection->login);
	}

	if (connection->password) {
		efree(connection->password);
	}

	zend_object_std_dtor(&connection->zo TSRMLS_CC);

	efree(object);
}

zend_object_value amqp_connection_ctor(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value new_value;

	amqp_connection_object* connection = (amqp_connection_object*)emalloc(sizeof(amqp_connection_object));
	memset(connection, 0, sizeof(amqp_connection_object));

	zend_object_std_init(&connection->zo, ce TSRMLS_CC);
	AMQP_OBJECT_PROPERTIES_INIT(connection->zo, ce);

	new_value.handle = zend_objects_store_put(
		connection,
		(zend_objects_store_dtor_t)zend_objects_destroy_object,
		(zend_objects_free_object_storage_t)amqp_connection_dtor,
		NULL TSRMLS_CC
	);

#if PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION >= 3
	memcpy((void *)&amqp_connection_object_handlers, (void *)zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	amqp_connection_object_handlers.get_debug_info = amqp_connection_object_get_debug_info;
	new_value.handlers = &amqp_connection_object_handlers;
#else
	new_value.handlers = zend_get_std_object_handlers();
#endif

	ALLOC_HASHTABLE(connection->channels_hashtable);
	zend_hash_init(connection->channels_hashtable, 0, NULL, NULL, 0);

	return new_value;
}


/* {{{ proto AMQPConnection::__construct([array optional])
 * The array can contain 'host', 'port', 'login', 'password', 'vhost', 'read_timeout', 'write_timeout', 'connect_timeout' and 'timeout' (deprecated) indexes
 */
PHP_METHOD(amqp_connection_class, __construct)
{
	zval *id;
	amqp_connection_object *connection;

	zval* ini_arr = NULL;
	zval** zdata;

	/* Parse out the method parameters */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O|a", &id, amqp_connection_class_entry, &ini_arr) == FAILURE) {
		return;
	}

	/* Pull the connection off of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Pull the login out of the $params array */
	zdata = NULL;
	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "login", sizeof("login"), (void*)&zdata)) {
		convert_to_string(*zdata);
	}
	/* Validate the given login */
	if (zdata && Z_STRLEN_PP(zdata) > 0) {
		if (Z_STRLEN_PP(zdata) < 128) {
			connection->login = estrndup(Z_STRVAL_PP(zdata), Z_STRLEN_PP(zdata));
		} else {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'login' exceeds 128 character limit.", 0 TSRMLS_CC);
			return;
		}
	} else {
		connection->login = estrndup(INI_STR("amqp.login"), strlen(INI_STR("amqp.login")) > 128 ? 128 : strlen(INI_STR("amqp.login")));
	}
	/* @TODO: write a macro to reduce code duplication */

	/* Pull the password out of the $params array */
	zdata = NULL;
	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF(ini_arr), "password", sizeof("password"), (void*)&zdata)) {
		convert_to_string(*zdata);
	}
	/* Validate the given password */
	if (zdata && Z_STRLEN_PP(zdata) > 0) {
		if (Z_STRLEN_PP(zdata) < 128) {
			connection->password = estrndup(Z_STRVAL_PP(zdata), Z_STRLEN_PP(zdata));
		} else {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'password' exceeds 128 character limit.", 0 TSRMLS_CC);
			return;
		}
	} else {
		connection->password = estrndup(INI_STR("amqp.password"), strlen(INI_STR("amqp.password")) > 128 ? 128 : strlen(INI_STR("amqp.password")));
	}

	/* Pull the host out of the $params array */
	zdata = NULL;
	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF(ini_arr), "host", sizeof("host"), (void *)&zdata)) {
		convert_to_string(*zdata);
	}
	/* Validate the given host */
	if (zdata && Z_STRLEN_PP(zdata) > 0) {
		if (Z_STRLEN_PP(zdata) < 128) {
			connection->host = estrndup(Z_STRVAL_PP(zdata), Z_STRLEN_PP(zdata));
		} else {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'host' exceeds 128 character limit.", 0 TSRMLS_CC);
			return;
		}
	} else {
		connection->host = estrndup(INI_STR("amqp.host"), strlen(INI_STR("amqp.host")) > 128 ? 128 : strlen(INI_STR("amqp.host")));
	}

	/* Pull the vhost out of the $params array */
	zdata = NULL;
	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "vhost", sizeof("vhost"), (void*)&zdata)) {
		convert_to_string(*zdata);
	}
	/* Validate the given vhost */
	if (zdata && Z_STRLEN_PP(zdata) > 0) {
		if (Z_STRLEN_PP(zdata) < 128) {
			connection->vhost = estrndup(Z_STRVAL_PP(zdata), Z_STRLEN_PP(zdata));
		} else {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'vhost' exceeds 128 character limit.", 0 TSRMLS_CC);
			return;
		}
	} else {
		connection->vhost = estrndup(INI_STR("amqp.vhost"), strlen(INI_STR("amqp.vhost")) > 128 ? 128 : strlen(INI_STR("amqp.vhost")));
	}

	connection->port = INI_INT("amqp.port");

	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "port", sizeof("port"), (void*)&zdata)) {
		convert_to_long(*zdata);
		connection->port = (size_t)Z_LVAL_PP(zdata);
	}

	connection->read_timeout = INI_FLT("amqp.read_timeout");

	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "read_timeout", sizeof("read_timeout"), (void*)&zdata)) {
		convert_to_double(*zdata);
		if (Z_DVAL_PP(zdata) < 0) {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'read_timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		} else {
			connection->read_timeout = Z_DVAL_PP(zdata);
		}

		if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "timeout", sizeof("timeout"), (void*)&zdata)) {
			/* 'read_timeout' takes precedence on 'timeout' but users have to know this */
			php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Parameter 'timeout' is deprecated, 'read_timeout' used instead");
		}

	} else if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "timeout", sizeof("timeout"), (void*)&zdata)) {

		php_error_docref(NULL TSRMLS_CC, E_DEPRECATED, "Parameter 'timeout' is deprecated; use 'read_timeout' instead");

		convert_to_double(*zdata);
		if (Z_DVAL_PP(zdata) < 0) {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		} else {
			connection->read_timeout = Z_DVAL_PP(zdata);
		}
	} else {

	   assert(DEFAULT_TIMEOUT == NULL);
	   if (DEFAULT_TIMEOUT != INI_STR("amqp.timeout")) {
		   php_error_docref(NULL TSRMLS_CC, E_DEPRECATED, "INI setting 'amqp.timeout' is deprecated; use 'amqp.read_timeout' instead");

		   assert(DEFAULT_READ_TIMEOUT != NULL);
		   if (!strcmp(DEFAULT_READ_TIMEOUT, INI_STR("amqp.read_timeout"))) {
				connection->read_timeout = INI_FLT("amqp.timeout");
		   } else {
				php_error_docref(NULL TSRMLS_CC, E_NOTICE, "INI setting 'amqp.read_timeout' will be used instead of 'amqp.timeout'");
				connection->read_timeout = INI_FLT("amqp.read_timeout");
		   }
	   } else {
			connection->read_timeout = INI_FLT("amqp.read_timeout");
	   }
	}

	connection->write_timeout = INI_FLT("amqp.write_timeout");

	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "write_timeout", sizeof("write_timeout"), (void*)&zdata)) {
		convert_to_double(*zdata);
		if (Z_DVAL_PP(zdata) < 0) {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'write_timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		} else {
			connection->write_timeout = Z_DVAL_PP(zdata);
		}
	}

	connection->connect_timeout = INI_FLT("amqp.connect_timeout");

	if (ini_arr && SUCCESS == zend_hash_find(HASH_OF (ini_arr), "connect_timeout", sizeof("connect_timeout"), (void*)&zdata)) {
		convert_to_double(*zdata);
		if (Z_DVAL_PP(zdata) < 0) {
			zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'connect_timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		} else {
			connection->connect_timeout = Z_DVAL_PP(zdata);
		}
	}
}
/* }}} */


/* {{{ proto amqp::isConnected()
check amqp connection */
PHP_METHOD(amqp_connection_class, isConnected)
{
	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* If the channel_connect is 1, we have a connection */
	if (connection->is_connected == '\1') {
		RETURN_TRUE;
	}

	/* We have no connection */
	RETURN_FALSE;
}
/* }}} */


/* {{{ proto amqp::connect()
create amqp connection */
PHP_METHOD(amqp_connection_class, connect)
{
	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (connection->is_connected) {

		assert(connection->connection_resource != NULL);
		if (connection->connection_resource->is_persistent) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to start transient connection while persistent transient one already established. Continue.");
		}

		RETURN_TRUE;
	}

	/* Actually connect this resource to the broker */
	RETURN_BOOL(php_amqp_connect(connection, 0 TSRMLS_CC));
}
/* }}} */


/* {{{ proto amqp::connect()
create amqp connection */
PHP_METHOD(amqp_connection_class, pconnect)
{
//	printf("pconnect called\n");

	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (connection->is_connected) {

		assert(connection->connection_resource != NULL);
		if (!connection->connection_resource->is_persistent) {
    		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to start persistent connection while transient one already established. Continue.");
    	}

		RETURN_TRUE;
	}

	/* Actually connect this resource to the broker or use stored connection */
	RETURN_BOOL(php_amqp_connect(connection, 1 TSRMLS_CC));
}
/* }}} */


/* {{{ proto amqp:pdisconnect()
destroy amqp persistent connection */
PHP_METHOD(amqp_connection_class, pdisconnect)
{
//	printf("pdisconnect called\n");


	zval *id;
	amqp_connection_object *connection;


	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (!connection->is_connected) {
		RETURN_TRUE;
	}

	assert(connection->connection_resource != NULL);

	if (!connection->connection_resource->is_persistent) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to close persistent connection while transient one already established. Abort.");

		RETURN_FALSE;
	}

	connection->connection_resource->is_persistent = 0;

	php_amqp_disconnect(connection TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto amqp::disconnect()
destroy amqp connection */
PHP_METHOD(amqp_connection_class, disconnect)
{
	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (!connection->is_connected) {
		RETURN_TRUE;
	}

	if (connection->connection_resource->is_persistent) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to close transient connection while persistent one already established. Abort.");

		RETURN_FALSE;
	}

	assert(connection->connection_resource != NULL);

	php_amqp_disconnect(connection TSRMLS_CC);

	RETURN_TRUE;
}

/* }}} */

/* {{{ proto amqp::reconnect()
recreate amqp connection */
PHP_METHOD(amqp_connection_class, reconnect)
{
	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (connection->is_connected == '\1') {

    	assert(connection->connection_resource != NULL);

    	if (connection->connection_resource->is_persistent) {
    		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to reconnect persistent connection while transient one already established. Abort.");

    		RETURN_FALSE;
    	}

		php_amqp_disconnect(connection TSRMLS_CC);
	}

	RETURN_BOOL(php_amqp_connect(connection, 0 TSRMLS_CC));
}
/* }}} */

/* {{{ proto amqp::preconnect()
recreate amqp connection */
PHP_METHOD(amqp_connection_class, preconnect)
{
	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);


	if (connection->is_connected == '\1') {

    	assert(connection->connection_resource != NULL);

    	if (!connection->connection_resource->is_persistent) {
    		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Attempt to reconnect transient connection while persistent one already established. Abort.");

    		RETURN_FALSE;
    	}

		connection->connection_resource->is_persistent = 0;

		php_amqp_disconnect(connection TSRMLS_CC);
	}

	RETURN_BOOL(php_amqp_connect(connection, 1 TSRMLS_CC));
}
/* }}} */


/* {{{ proto amqp::getLogin()
get the login */
PHP_METHOD(amqp_connection_class, getLogin)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the login from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the login to the amqp object */
	RETURN_STRING(connection->login, 1);
}
/* }}} */


/* {{{ proto amqp::setLogin(string login)
set the login */
PHP_METHOD(amqp_connection_class, setLogin)
{
	zval *id;
	amqp_connection_object *connection;
	char *login;
	int login_len;

	/* @TODO: use macro when one is created for constructor */
	/* Get the login from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &id, amqp_connection_class_entry, &login, &login_len) == FAILURE) {
		return;
	}

	/* Validate login length */
	if (login_len > 128) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Invalid 'login' given, exceeds 128 characters limit.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Free previously existing login, in cases where setLogin() is called multiple times */
	if (connection->login) {
		efree(connection->login);
	}

	/* Copy the login to the amqp object */
	connection->login = estrndup(login, login_len);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto amqp::getPassword()
get the password */
PHP_METHOD(amqp_connection_class, getPassword)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the password from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the password to the amqp object */
	RETURN_STRING(connection->password, 1);
}
/* }}} */


/* {{{ proto amqp::setPassword(string password)
set the password */
PHP_METHOD(amqp_connection_class, setPassword)
{
	zval *id;
	amqp_connection_object *connection;
	char *password;
	int password_len;

	/* Get the password from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &id, amqp_connection_class_entry, &password, &password_len) == FAILURE) {
		return;
	}

	/* Validate password length */
	if (password_len > 128) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Invalid 'password' given, exceeds 128 characters limit.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Free previously existing password, in cases where setPassword() is called multiple times */
	if (connection->password) {
		efree(connection->password);
	}

	/* Copy the password to the amqp object */
	connection->password = estrndup(password, password_len);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto amqp::getHost()
get the host */
PHP_METHOD(amqp_connection_class, getHost)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the host from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the host to the amqp object */
	RETURN_STRING(connection->host, 1);
}
/* }}} */


/* {{{ proto amqp::setHost(string host)
set the host */
PHP_METHOD(amqp_connection_class, setHost)
{
	zval *id;
	amqp_connection_object *connection;
	char *host;
	int host_len;

	/* Get the host from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &id, amqp_connection_class_entry, &host, &host_len) == FAILURE) {
		return;
	}

	/* Validate host length */
	if (host_len > 1024) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Invalid 'host' given, exceeds 1024 character limit.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Free previously existing host, in cases where setHost() is called multiple times */
	if (connection->host) {
		efree(connection->host);
	}

	/* Copy the host to the amqp object */
	connection->host = estrndup(host, host_len);

	RETURN_TRUE;
}
/* }}} */


/* {{{ proto amqp::getPort()
get the port */
PHP_METHOD(amqp_connection_class, getPort)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the port from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the port to the amqp object */
	RETURN_LONG(connection->port);
}
/* }}} */


/* {{{ proto amqp::setPort(mixed port)
set the port */
PHP_METHOD(amqp_connection_class, setPort)
{
	zval *id;
	amqp_connection_object *connection;
	zval *zvalPort;
	int port;

	/* Get the port from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oz", &id, amqp_connection_class_entry, &zvalPort) == FAILURE) {
		return;
	}

	/* Parse out the port*/
	switch (Z_TYPE_P(zvalPort)) {
		case IS_DOUBLE:
			port = (int)Z_DVAL_P(zvalPort);
			break;
		case IS_LONG:
			port = (int)Z_LVAL_P(zvalPort);
			break;
		case IS_STRING:
			convert_to_long(zvalPort);
			port = (int)Z_LVAL_P(zvalPort);
			break;
		default:
			port = 0;
	}

	/* Check the port value */
	if (port <= 0 || port > 65535) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Invalid port given. Value must be between 1 and 65535.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the port to the amqp object */
	connection->port = port;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto amqp::getVhost()
get the vhost */
PHP_METHOD(amqp_connection_class, getVhost)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the vhost from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the vhost to the amqp object */
	RETURN_STRING(connection->vhost, 1);
}
/* }}} */


/* {{{ proto amqp::setVhost(string vhost)
set the vhost */
PHP_METHOD(amqp_connection_class, setVhost)
{
	zval *id;
	amqp_connection_object *connection;
	char *vhost;
	int vhost_len;

	/* Get the vhost from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &id, amqp_connection_class_entry, &vhost, &vhost_len) == FAILURE) {
		return;
	}

	/* Validate vhost length */
	if (vhost_len > 128) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'vhost' exceeds 128 characters limit.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Free previously existing vhost, in cases where setVhost() is called multiple times */
	if (connection->vhost) {
		efree(connection->vhost);
	}

	/* Copy the vhost to the amqp object */
	connection->vhost = estrndup(vhost, vhost_len);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto amqp::getTimeout()
@deprecated
get the timeout */
PHP_METHOD(amqp_connection_class, getTimeout)
{
	zval *id;
	amqp_connection_object *connection;

	php_error_docref(NULL TSRMLS_CC, E_DEPRECATED, "AMQPConnection::getTimeout() method is deprecated; use AMQPConnection::getReadTimeout() instead");

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	RETURN_DOUBLE(connection->read_timeout);
}
/* }}} */

/* {{{ proto amqp::setTimeout(double timeout)
@deprecated
set the timeout */
PHP_METHOD(amqp_connection_class, setTimeout)
{
	zval *id;
	amqp_connection_object *connection;
	double read_timeout;

	php_error_docref(NULL TSRMLS_CC, E_DEPRECATED, "AMQPConnection::setTimeout($timeout) method is deprecated; use AMQPConnection::setReadTimeout($timeout) instead");

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Od", &id, amqp_connection_class_entry, &read_timeout) == FAILURE) {
		return;
	}

	/* Validate timeout */
	if (read_timeout < 0) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	connection->read_timeout = read_timeout;

	if (connection->is_connected == '\1') {
		if (php_amqp_set_read_timeout(connection TSRMLS_CC) == 0) {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto amqp::getReadTimeout()
get the read timeout */
PHP_METHOD(amqp_connection_class, getReadTimeout)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	RETURN_DOUBLE(connection->read_timeout);
}
/* }}} */

/* {{{ proto amqp::setReadTimeout(double timeout)
set read timeout */
PHP_METHOD(amqp_connection_class, setReadTimeout)
{
	zval *id;
	amqp_connection_object *connection;
	double read_timeout;

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Od", &id, amqp_connection_class_entry, &read_timeout) == FAILURE) {
		return;
	}

	/* Validate timeout */
	if (read_timeout < 0) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'read_timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	connection->read_timeout = read_timeout;

	if (connection->is_connected == '\1') {
		if (php_amqp_set_read_timeout(connection TSRMLS_CC) == 0) {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto amqp::getWriteTimeout()
get write timeout */
PHP_METHOD(amqp_connection_class, getWriteTimeout)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	RETURN_DOUBLE(connection->write_timeout);
}
/* }}} */

/* {{{ proto amqp::setWriteTimeout(double timeout)
set write timeout */
PHP_METHOD(amqp_connection_class, setWriteTimeout)
{
	zval *id;
	amqp_connection_object *connection;
	double write_timeout;

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Od", &id, amqp_connection_class_entry, &write_timeout) == FAILURE) {
		return;
	}

	/* Validate timeout */
	if (write_timeout < 0) {
		zend_throw_exception(amqp_connection_exception_class_entry, "Parameter 'write_timeout' must be greater than or equal to zero.", 0 TSRMLS_CC);
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	connection->write_timeout = write_timeout;

	if (connection->is_connected == '\1') {
		if (php_amqp_set_write_timeout(connection TSRMLS_CC) == 0) {
			RETURN_FALSE;
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto amqp::getLastChannelId()
get write timeout */
PHP_METHOD(amqp_connection_class, getLastChannelId)
{
	zval *id;
	amqp_connection_object *connection;

	/* Get the timeout from the method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	/* Copy the timeout to the amqp object */
	RETURN_LONG(connection->connection_resource->last_channel_id);
}
/* }}} */

/* {{{ proto amqp::isPersistent()
check whether amqp connection is persistent */
PHP_METHOD(amqp_connection_class, isPersistent)
{
	zval *id;
	amqp_connection_object *connection;

	/* Try to pull amqp object out of method params */
	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &id, amqp_connection_class_entry) == FAILURE) {
		return;
	}

	/* Get the connection object out of the store */
	connection = (amqp_connection_object *)zend_object_store_get_object(id TSRMLS_CC);

	if (!connection->connection_resource) {
		/* We have no active connection resource */
		return;
	}

	RETURN_BOOL(connection->connection_resource->is_persistent);
}
/* }}} */


/*
*Local variables:
*tab-width: 4
*c-basic-offset: 4
*End:
*vim600: noet sw=4 ts=4 fdm=marker
*vim<6
*/
