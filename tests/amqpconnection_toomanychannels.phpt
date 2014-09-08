--TEST--
AMQPConnection too many channels on a connection
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->connect();

for ($i = 0; $i < PHP_AMQP_MAX_CHANNELS; $i++) {
	$channel = new AMQPChannel($cnn);
    //echo $channel->getChannelId(), PHP_EOL;
    $channel = null;
}

echo "Good\n";

try {
	new AMQPChannel($cnn);
	echo "Bad\n";
} catch(Exception $e) {
    echo get_class($e), ': ', $e->getMessage(), PHP_EOL;
}

?>
--EXPECT--
Good
AMQPChannelException: Could not create channel. Connection has no open channel slots remaining.