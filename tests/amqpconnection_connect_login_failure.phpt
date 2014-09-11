--TEST--
AMQPConnection connect login failure
--SKIPIF--
<?php if (!extension_loaded("amqp")) print "skip"; ?>
--FILE--
<?php
$cnn = new AMQPConnection();
$cnn->setLogin('nonexistent-login-'.time());
$cnn->setPassword('nonexistent-password-'.time());

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

?>
--EXPECT--
disconnected
AMQPConnectionException: Library error: connection closed unexpectedly - Potential login failure.
disconnected