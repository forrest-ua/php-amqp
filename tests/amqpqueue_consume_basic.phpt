--TEST--
AMQPQueue::consume basic
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->connect();

$ch = new AMQPChannel($cnn);

// Declare a new exchange
$ex = new AMQPExchange($ch);
$ex->setName('exchange-' . microtime(true));
$ex->setType(AMQP_EX_TYPE_FANOUT);
$ex->declareExchange();

// Create a new queue
$q = new AMQPQueue($ch);
$q->setName('queue-' . microtime(true));
$q->declareQueue();

// Bind it on the exchange to routing.key
$q->bind($ex->getName(), 'routing.*');

// Publish a message to the exchange with a routing key
$ex->publish('message1', 'routing.1', AMQP_NOPARAM, ['content_type' => 'plain/test', 'headers' => ['foo' => 'bar']]);
$ex->publish('message2', 'routing.2', AMQP_NOPARAM, ['delivery_mode' => AMQP_DURABLE]);
$ex->publish('message3', 'routing.3', AMQP_DURABLE); // this is wrong way to make messages persistent

$count = 0;

function consumeThings($message, $queue) {
	global $count;

    echo "call #$count", PHP_EOL;
    // Read from the queue
    var_dump($message);
	$count++;

	if ($count >= 2) {
		return false;
	}

	return true;
}

// Read from the queue
$q->consume("consumeThings", AMQP_AUTOACK);

$q->delete();
$ex->delete();

?>
--EXPECTF--
call #0
object(AMQPEnvelope)#5 (18) {
  ["body"]=>
  string(8) "message1"
  ["content_type"]=>
  string(10) "plain/test"
  ["routing_key"]=>
  string(9) "routing.1"
  ["delivery_tag"]=>
  int(1)
  ["delivery_mode"]=>
  int(0)
  ["exchange_name"]=>
  string(%d) "exchange-%f"
  ["is_redelivery"]=>
  int(0)
  ["content_encoding"]=>
  string(0) ""
  ["type"]=>
  string(0) ""
  ["timestamp"]=>
  int(0)
  ["priority"]=>
  int(0)
  ["expiration"]=>
  string(0) ""
  ["user_id"]=>
  string(0) ""
  ["app_id"]=>
  string(0) ""
  ["message_id"]=>
  string(0) ""
  ["reply_to"]=>
  string(0) ""
  ["correlation_id"]=>
  string(0) ""
  ["headers"]=>
  array(1) {
    ["foo"]=>
    string(3) "bar"
  }
}
call #1
object(AMQPEnvelope)#5 (18) {
  ["body"]=>
  string(8) "message2"
  ["content_type"]=>
  string(10) "text/plain"
  ["routing_key"]=>
  string(9) "routing.2"
  ["delivery_tag"]=>
  int(2)
  ["delivery_mode"]=>
  int(2)
  ["exchange_name"]=>
  string(%d) "exchange-%f"
  ["is_redelivery"]=>
  int(0)
  ["content_encoding"]=>
  string(0) ""
  ["type"]=>
  string(0) ""
  ["timestamp"]=>
  int(0)
  ["priority"]=>
  int(0)
  ["expiration"]=>
  string(0) ""
  ["user_id"]=>
  string(0) ""
  ["app_id"]=>
  string(0) ""
  ["message_id"]=>
  string(0) ""
  ["reply_to"]=>
  string(0) ""
  ["correlation_id"]=>
  string(0) ""
  ["headers"]=>
  array(0) {
  }
}

