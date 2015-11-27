<?php

// Put your device token here (without spaces):
//$deviceToken = '0cf812d02f1e0df951851b8f0279b0a05b0157efc71f296ebf0b296ff328bf17';

// Put your private key's passphrase here:密语
$passphrase = '123qwe';

// Put your alert message here:
$message = '您收到一条来自 XXX 的消息';

$apnTestServer = 'ssl://gateway.sandbox.push.apple.com:2195';
$apnserver = 'ssl://gateway.push.apple.com:2195';

$deviceToken = $argv[1];
$message = base64_decode($argv[2]);

////////////////////////////////////////////////////////////////////////////////

$ctx = stream_context_create();
stream_context_set_option($ctx, 'ssl', 'local_cert', 'ck.pem');
stream_context_set_option($ctx, 'ssl', 'passphrase', $passphrase);

// Open a connection to the APNS server
$fp = stream_socket_client(
	$apnTestServer, $err,
	$errstr, 60, STREAM_CLIENT_CONNECT|STREAM_CLIENT_PERSISTENT, $ctx);

if (!$fp)
	exit("Failed to connect: $err $errstr" . PHP_EOL);

echo 'Connected to APNS' . PHP_EOL;

// Create the payload body
$body['aps'] = array(
	'alert' => $message,
	'sound' => 'default'
	);

// Encode the payload as JSON
$payload = json_encode($body);

// Build the binary notification
$msg = chr(0) . pack('n', 32) . pack('H*', $deviceToken) . pack('n', strlen($payload)) . $payload;

// Send it to the server
$result = fwrite($fp, $msg, strlen($msg));

if (!$result)
	echo 'Message not delivered' . PHP_EOL;
else
	echo 'Message successfully delivered' . PHP_EOL;

// Close the connection to the server
fclose($fp);
    
?>
