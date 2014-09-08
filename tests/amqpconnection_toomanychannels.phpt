--TEST--
AMQPConnection too many channels on a connection
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->connect();

$channels = array();

for ($i = 0; $i < PHP_AMQP_MAX_CHANNELS; $i++) {
	$channels[$i] = new AMQPChannel($cnn);
}

echo "Good\n";

try {
	new AMQPChannel($cnn);
	echo "Bad\n";
} catch(Exception $e) {
	echo "Caught!";
}

?>
--EXPECT--
Good
Caught!