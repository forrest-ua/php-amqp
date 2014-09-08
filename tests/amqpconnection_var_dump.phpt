--TEST--
AMQPConnection var_dump
--SKIPIF--
<?php
if (!extension_loaded("amqp") || version_compare(PHP_VERSION, '5.3', '<')) {
  print "skip";
}
?>
--FILE--
<?php
$cnn = new AMQPConnection();
var_dump($cnn);
$cnn->connect();
$cnn->connect();
var_dump($cnn);

$c = new AMQPChannel($cnn);

var_dump($cnn);

$cnn->disconnect();
var_dump($cnn);
?>
--EXPECT--
object(AMQPConnection)#1 (12) {
  ["login"]=>
  string(5) "guest"
  ["password"]=>
  string(5) "guest"
  ["host"]=>
  string(9) "localhost"
  ["vhost"]=>
  string(1) "/"
  ["port"]=>
  int(5672)
  ["read_timeout"]=>
  float(0)
  ["write_timeout"]=>
  float(0)
  ["connect_timeout"]=>
  float(0)
  ["is_connected"]=>
  bool(false)
  ["connection_resource"]=>
  NULL
  ["is_persistent"]=>
  NULL
  ["last_channel_id"]=>
  NULL
}
object(AMQPConnection)#1 (12) {
  ["login"]=>
  string(5) "guest"
  ["password"]=>
  string(5) "guest"
  ["host"]=>
  string(9) "localhost"
  ["vhost"]=>
  string(1) "/"
  ["port"]=>
  int(5672)
  ["read_timeout"]=>
  float(0)
  ["write_timeout"]=>
  float(0)
  ["connect_timeout"]=>
  float(0)
  ["is_connected"]=>
  bool(true)
  ["connection_resource"]=>
  resource(4) of type (AMQP Connection Resource)
  ["is_persistent"]=>
  bool(false)
  ["last_channel_id"]=>
  int(0)
}
object(AMQPConnection)#1 (12) {
  ["login"]=>
  string(5) "guest"
  ["password"]=>
  string(5) "guest"
  ["host"]=>
  string(9) "localhost"
  ["vhost"]=>
  string(1) "/"
  ["port"]=>
  int(5672)
  ["read_timeout"]=>
  float(0)
  ["write_timeout"]=>
  float(0)
  ["connect_timeout"]=>
  float(0)
  ["is_connected"]=>
  bool(true)
  ["connection_resource"]=>
  resource(4) of type (AMQP Connection Resource)
  ["is_persistent"]=>
  bool(false)
  ["last_channel_id"]=>
  int(1)
}
object(AMQPConnection)#1 (12) {
  ["login"]=>
  string(5) "guest"
  ["password"]=>
  string(5) "guest"
  ["host"]=>
  string(9) "localhost"
  ["vhost"]=>
  string(1) "/"
  ["port"]=>
  int(5672)
  ["read_timeout"]=>
  float(0)
  ["write_timeout"]=>
  float(0)
  ["connect_timeout"]=>
  float(0)
  ["is_connected"]=>
  bool(false)
  ["connection_resource"]=>
  NULL
  ["is_persistent"]=>
  NULL
  ["last_channel_id"]=>
  NULL
}