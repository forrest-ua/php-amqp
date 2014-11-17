--TEST--
AMQPConnection persitent connection are reusable
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->pconnect();
echo get_class($cnn), PHP_EOL;
echo $cnn->isConnected() ? 'true' : 'false', PHP_EOL;

var_dump($cnn);

echo PHP_EOL;

$cnn = new AMQPConnection();
$cnn->pconnect();
echo get_class($cnn), PHP_EOL;
echo $cnn->isConnected() ? 'true' : 'false', PHP_EOL;
var_dump($cnn);

$cnn->pdisconnect();

?>
--EXPECT--
AMQPConnection
true
object(AMQPConnection)#1 (13) {
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
  ["is_persistent"]=>
  bool(true)
  ["connection_resource"]=>
  resource(4) of type (AMQP Connection Resource)
  ["used_channels"]=>
  int(0)
  ["max_channel_id"]=>
  int(256)
}

AMQPConnection
true
object(AMQPConnection)#2 (13) {
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
  ["is_persistent"]=>
  bool(true)
  ["connection_resource"]=>
  resource(5) of type (AMQP Connection Resource)
  ["used_channels"]=>
  int(0)
  ["max_channel_id"]=>
  int(256)
}