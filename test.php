<?php
$cnn = new AMQPConnection();
$cnn->connect();

var_dump($cnn);die;
//for ($i = 0; $i < PHP_AMQP_MAX_CHANNELS; $i++) {
//for ($i = 0; $i < 1000; $i++) {
    $channel = new AMQPChannel($cnn);
    //echo $channel->getChannelId(), PHP_EOL;
    echo 'destroy channel', PHP_EOL;
    $channel = null;
//}


echo "Good\n";

try {
    new AMQPChannel($cnn);
    echo "Bad\n";
} catch(Exception $e) {
    echo get_class($e), ': ', $e->getMessage(), PHP_EOL;
}

echo "DONE", PHP_EOL;