<?php
//var_dump(memory_get_usage(true), memory_get_usage());

$conn = new AMQPConnection();

//var_dump($conn);

//debug_zval_dump($conn);
$conn->connect();
//debug_zval_dump($conn);


$ch = new AMQPChannel($conn);
//debug_zval_dump($ch);

var_dump($conn->getLastChannelId());

$conn->disconnect();

//echo "Going to kill resource", PHP_EOL;
//$conn->disconnect();
//echo "Resource should be killed", PHP_EOL;
//$conn->disconnect();
//echo "Resource should be killed", PHP_EOL;
//
$ch=null;
$conn = null;

//var_dump(memory_get_usage(true), memory_get_usage());

echo "DONE", PHP_EOL;