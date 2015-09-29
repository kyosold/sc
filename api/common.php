<?php

session_start();

define('ENV_PATH', $_SERVER['DOCUMENT_ROOT']);
define('HTTP_HOST', $_SERVER['HTTP_HOST']);

define('STEMPLATE_PATH', ENV_PATH."/templates/");
define('STEMPLATEC_PATH', ENV_PATH."/templates_c/");
define('SCONFIG_PATH', ENV_PATH."/config/");
define('SCACHE_PATH', ENV_PATH."/cache/");

define('DATA_PATH', ENV_PATH."/data/");
define('DATA_HOST', "http://".HTTP_HOST."/data/");

// APNS
define('APNS_URI', ENV_PATH."/api/push.php");

// Log
define('LOG_FILE', ENV_PATH.'/api/logs/access.log');

// Database info
define('DB_HOST', 'localhost');
define('DB_PORT', '3306');
define('DB_USER', 'root');
define('DB_PWD', '1234qwer');
define('DB_NAME', 'sc');

$PDO_DB_DSN = "mysql:host=".DB_HOST.";port=".DB_PORT.";dbname=".DB_NAME.";charset=utf8";

// MC Info
define('MC_HOST', '127.0.0.1');
define('MC_PORT', 11234);
define('MC_TIMEOUT', 5);

// SOCKET
define("SOCK_HOST", "icarot.com");
define("SOCK_PORT", 5027);
define("SOCK_TIMEOUT", 3);

// Other Info
define('ROWS_OF_PAGE', 50);

define('MAX_PWD_NUM_NOR', 3);

// Error Code
define('2000', 'connect database fail');
define('2001', 'query database fail');

// Logic Error Code
define('4000', 'update table [liao_user] fail for update ios_token');
define('4001', 'write database table [m_cure] fail');
define('4002', 'delete database table [files] fail');
define('4003', 'delete database table [files] fid is not valid');
define('4004', 'can not create dir');
define('4005', 'can not find request to make friend');
define('4006', 'can not find account from liao_pwds');
define('4007', 'param error');
define('9000', 'baidu map api fail');
define('9101', 'set notice fail');

?>
