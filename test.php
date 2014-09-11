<?php
$cnn = new AMQPConnection();
$cnn->connect();

$ch = new AMQPChannel($cnn);

$e = new AMQPExchange($ch);

$e->publish('test', 'test');

var_dump($cnn);
var_dump($ch);