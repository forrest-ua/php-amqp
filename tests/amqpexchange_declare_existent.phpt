--TEST--
AMQPExchange
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->connect();

$ch = new AMQPChannel($cnn);

echo 'Channel id: ', $ch->getChannelId(), PHP_EOL;

$exchangge_name = "exchange-" . microtime(true);

$ex = new AMQPExchange($ch);
$ex->setName($exchangge_name);
$ex->setType(AMQP_EX_TYPE_FANOUT);
echo "Exchange declared: ", $ex->declareExchange() ? "true" : "false", PHP_EOL;

try {
    $ex = new AMQPExchange($ch);
    $ex->setName($exchangge_name);
    $ex->setType(AMQP_EX_TYPE_TOPIC);
    $ex->declareExchange();
} catch (AMQPException $e) {
    echo get_class($e), ': ', $e->getMessage(), PHP_EOL;
}

echo "Channel connected: ", $ch->isConnected() ? "true" : "false", PHP_EOL;
echo "Connection connected: ", $cnn->isConnected() ? "true" : "false", PHP_EOL;

try {
    $ex = new AMQPExchange($ch);
    echo "New exchange class created", PHP_EOL;
} catch (AMQPException $e) {
    echo get_class($e), ': ', $e->getMessage(), PHP_EOL;
}

?>
--EXPECTF--
Channel id: 1
Exchange declared: true
AMQPExchangeException: Server channel error: 406, message: PRECONDITION_FAILED - cannot redeclare exchange 'exchange-%f' in vhost '/' with different type, durable, internal or autodelete value
Channel connected: false
Connection connected: true
AMQPChannelException: Could not create exchange. No channel available.