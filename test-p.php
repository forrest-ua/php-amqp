<?php

var_dump(memory_get_usage(true), memory_get_usage());

$conn = new AMQPConnection();

//debug_zval_dump($conn);
$conn->pconnect();

var_dump($conn->getLastChannelId());

$ch = new AMQPChannel($conn);

//$conn->connect();
//echo "Going to kill persistent resource", PHP_EOL;
//$conn->pdisconnect();
//echo "Persistent resource should be killed", PHP_EOL;



$conn = null;

$ch=null;


var_dump(memory_get_usage(true), memory_get_usage());

echo "DONE", PHP_EOL;