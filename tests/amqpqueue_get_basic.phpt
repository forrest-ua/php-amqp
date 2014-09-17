--TEST--
AMQPQueue::get basic
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->connect();

$ch = new AMQPChannel($cnn);

// Declare a new exchange
$ex = new AMQPExchange($ch);
$ex->setName('exchange1');
$ex->setType(AMQP_EX_TYPE_FANOUT);
$ex->declareExchange();

// Create a new queue
$q = new AMQPQueue($ch);
$q->setName('queue1' . microtime(true));
$q->declareQueue();

// Bind it on the exchange to routing.key
$q->bind($ex->getName(), 'routing.*');
// Publish a message to the exchange with a routing key
$ex->publish('message1', 'routing.1', AMQP_NOPARAM, ['content_type' => 'plain/test', 'headers' => ['foo' => 'bar']]);
$ex->publish('message2', 'routing.2', AMQP_DURABLE);
$ex->publish('message3', 'routing.3');


for ($i = 0; $i < 4; $i++) {
    echo "call #$i", PHP_EOL;
	// Read from the queue
	$msg = $q->get(AMQP_AUTOACK);
    var_dump($msg);
}

$ex->delete();
?>
--EXPECT--
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
  string(9) "exchange1"
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
object(AMQPEnvelope)#6 (18) {
  ["body"]=>
  string(8) "message2"
  ["content_type"]=>
  string(10) "text/plain"
  ["routing_key"]=>
  string(9) "routing.2"
  ["delivery_tag"]=>
  int(2)
  ["delivery_mode"]=>
  int(0)
  ["exchange_name"]=>
  string(9) "exchange1"
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
call #2
object(AMQPEnvelope)#5 (18) {
  ["body"]=>
  string(8) "message3"
  ["content_type"]=>
  string(10) "text/plain"
  ["routing_key"]=>
  string(9) "routing.3"
  ["delivery_tag"]=>
  int(3)
  ["delivery_mode"]=>
  int(0)
  ["exchange_name"]=>
  string(9) "exchange1"
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
call #3
bool(false)
