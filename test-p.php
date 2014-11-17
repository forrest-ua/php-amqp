<?php

//var_dump(memory_get_usage(true), memory_get_usage());

$conn = new AMQPConnection();

//debug_zval_dump($conn);
$conn->pconnect();

$ch = new AMQPChannel($conn);
var_dump($conn);


$conn2 = new AMQPConnection();
$conn2->connect();
var_dump($conn2);

$ch=null;
$conn=null;
$conn2= null;

$conn3 = new AMQPConnection();

//debug_zval_dump($conn);
$conn3->pconnect();

var_dump($conn3);

//var_dump(memory_get_usage(true), memory_get_usage());

echo "DONE", PHP_EOL;