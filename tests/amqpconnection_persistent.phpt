--TEST--
AMQPConnection persistent connections
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->pconnect();
var_dump($cnn);

$cnn2 = new AMQPConnection(['login'=>'test', 'password'=>'test']);
$cnn2->pconnect();
var_dump($cnn2);

$cnn->pdisconnect();
var_dump($cnn);
var_dump($cnn2);

$cnn2->pdisconnect();
var_dump($cnn2);
var_dump($cnn);

$cnn = null;
$cnn2 = null;

echo 'DONE';

?>
--EXPECT--
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
  ["connection_resource"]=>
  resource(4) of type (AMQP Connection Resource)
  ["is_persistent"]=>
  bool(true)
  ["used_channels"]=>
  int(0)
  ["max_channel_id"]=>
  int(256)
}
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
  ["connection_resource"]=>
  resource(5) of type (AMQP Connection Resource)
  ["is_persistent"]=>
  bool(true)
  ["used_channels"]=>
  int(0)
  ["max_channel_id"]=>
  int(256)
}
