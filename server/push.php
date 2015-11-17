#!/usr/local/webserver/php/bin/php
<?php

// Put your device token here (without spaces):
//$deviceToken = '0cf812d02f1e0df951851b8f0279b0a05b0157efc71f296ebf0b296ff328bf17';

// Put your private key's passphrase here:密语
$passphrase = '123qwe';

// Put your alert message here:
//$message = '您收到一条来自 XXX 的消息';

$isTest = 1;


$apnTestServer = 'ssl://gateway.sandbox.push.apple.com:2195';
$apnserver = 'ssl://gateway.push.apple.com:2195';

if ($argc != 6) {
	echo "push.php param error";
	return;
}

$deviceToken = $argv[1];
$fuid = $argv[2];
$fnick = $argv[3];
$ftype = $argv[4];
$tuid = $argv[5];
$message = '您收到一条来自 '. base64_decode($fnick) .' 的消息';

////////////////////////////////////////////////////////////////////////////////

/*$ctx = stream_context_create();
//stream_context_set_option($ctx, 'ssl', 'local_cert', 'ck_dev.pem');
stream_context_set_option($ctx, 'ssl', 'local_cert', 'ck_pro.pem');
stream_context_set_option($ctx, 'ssl', 'passphrase', $passphrase);

// Open a connection to the APNS server
$fp = stream_socket_client(
	//$apnTestServer, $err,
	$apnserver, $err,
	$errstr, 60, STREAM_CLIENT_CONNECT|STREAM_CLIENT_PERSISTENT, $ctx);*/

$ctxTest = stream_context_create();
stream_context_set_option($ctxTest, 'ssl', 'local_cert', 'ck_dev.pem');
stream_context_set_option($ctxTest, 'ssl', 'passphrase', $passphrase);

$fpTest = stream_socket_client(
	$apnTestServer, $err,
	$errstr, 60, STREAM_CLIENT_CONNECT|STREAM_CLIENT_PERSISTENT, $ctxTest);

$ctxPro = stream_context_create();
stream_context_set_option($ctxPro, 'ssl', 'local_cert', 'ck_pro.pem');
stream_context_set_option($ctxPro, 'ssl', 'passphrase', $passphrase);

$fpPro = stream_socket_client(
	$apnserver, $err,
	$errstr, 60, STREAM_CLIENT_CONNECT|STREAM_CLIENT_PERSISTENT, $ctxPro);

if ($isTest == 1) {
	$fp = $fpTest;
} else {
	$fp = $fpPro;
}


if (!$fp)
	exit("Failed to connect: $err $errstr" . PHP_EOL);

echo 'Connected to APNS' . PHP_EOL;

// Create the payload body
$body['aps'] = array(
	'fuid'	=> $fuid,
	'fnick' => $fnick,
	'type' => $ftype,
	'tuid' => $tuid,
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
