<?php
include_once('common.php');

// avoid sql
function stripslashes_deep($value)
{
	$value = is_array($value) ? array_map('stripslashes_deep', $value) : (isset($value) ? stripslashes($value) : null);
}
// use $_POST = stripslashes_deep($_POS);

function avoid_sql($value)
{
	// php 5.4 use pdo, don't use this
	return $value;

    // 去除斜杠
    if (get_magic_quotes_gpc()) {
        $value = stripslashes($value);
    }   

    if (!is_numeric($value)) {
        $value = mysql_real_escape_string($value);
    }    

    return $value;  
}

function log_info($str)
{
	$curr_date = date("Y/m/d H:i:s");
	$exec_name = $_SERVER['REQUEST_URI'];

	$mess = "{$curr_date} {$exec_name}: {$str}\n";
	error_log($mess, 3, LOG_FILE);
}


function show_info($status, $desc)
{
	$res = array();
	$res['status'] = $status;
	$res['des'] = $desc;
	
	return $res;
}


function mk_dir($dir, $mode = 0755)
{
	if (is_dir($dir) || @mkdir($dir, $mode))
		return TRUE;

	if (!mk_dir(dirname($dir), $mode))
		return FALSE;

	return @mkdir($dir, $mode); 
}


function get_avatar_url($uid)
{
	return DATA_HOST ."/". $uid ."/avatar/s_avatar.jpg";
}

?>
