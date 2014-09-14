--TEST--
AMQPConnection connect login failure
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
//ini_set('amqp.connect_timeout', 60);
//ini_set('amqp.read_timeout', 60);
//ini_set('amqp.write_timeout', 60);

$cnn = new AMQPConnection();
$cnn->setLogin('nonexistent-login-'.time());
$cnn->setPassword('nonexistent-password-'.time());

//var_dump($cnn);

echo ($cnn->isConnected() ? 'connected' : 'disconnected'), PHP_EOL;
//
try {
    $cnn->connect();
    echo 'Connected', PHP_EOL;
} catch (AMQPException $e) {
    echo get_class($e), ': ', $e->getMessage(), PHP_EOL;
}
//
echo ($cnn->isConnected() ? 'connected' : 'disconnected'), PHP_EOL;

// NOTE: on travis CI this following expectation fails:
//
//--EXPECT--
//disconnected
//AMQPConnectionException: Library error: connection closed unexpectedly - Potential login failure.
//disconnected
//
// with:
//
//002+ AMQPConnectionException: Library error: a socket error occurred - Potential login failure.
//002- AMQPConnectionException: Library error: connection closed unexpectedly - Potential login failure.
?>
--EXPECT--
disconnected
AMQPConnectionException: Library error: connection closed unexpectedly - Potential login failure.
disconnected