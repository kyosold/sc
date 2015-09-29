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

// return bool
function send_notice_with_apns($fuid, $fnick, $tuid, $tag_type)
{
	// 获取收件人的ios_token
	$ios_token = '';
	try {
		$db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
            
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);
        
        $sql = "SELECT ios_token FROM sc_logined WHERE uid = {$tuid} LIMIT 1";
        $stmt = $db->prepare($sql);
        $stmt->execute();
        if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$ios_token = $row['ios_token']; 
		} else {
			log_info("user:{$tuid} is not set apns");
			return false;
		}
		
		// 发送apns
		$apn_cmd = APNS_URI." {$ios_token} {$fuid} {$fnick} {$tag_type} {$tuid}";
		$apn_fp = popen($apn_cmd, "r");
		while (!feof($apn_fp)) {
			$apn_res .= fgets($apn_fp);
		}
		pclose($apn_fp);
		
		log_info("APNS result:{$apn_res}");
		
		return true;
		
	} catch (PDOException $e) {
		log_error("send apns to tuid:{$tuid} fail:". $e->getMessage());
		
		return false;
	}
}

function send_notice_with_socket($tpid, $fuid, $fnick, $tuid, $tag_type)
{
	try {
		$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
		
		// set socket receive timeout to 5 second
		socket_set_option($socket, SOL_SOCKET, SO_RCVTIMEO, array("sec"=>SOCK_TIMEOUT, "usec"=>0));

		$connect = socket_connect($socket, SOCK_HOST, SOCK_PORT); // 连接服务器
		if ($connect == false) {
			log_error("socket connect ". SOCK_HOST." ".SOCK_PORT." fail:". socket_strerror());
			return false;
		}
		
		// send cmd
		$wbuf = "SENDNOTICETOOTHERPROGRESS pid:{$tpid} fuid:{$fuid} fnick:{$fnick} mtype:{$tag_type} tuid:{$tuid} \r\n\r\n";
		$wn = socket_write($socket, $wbuf);
		if ($wn === false) {
			log_error("socket write fail:". socket_strerror());
			
			socket_close($socket);
			return false;
		}
		
		$buffer = '';
		
		do {
			
			if ( false === ($buffer = socket_read($socket, 2048))) {
				log_error("socket_read faild:". socket_strerror(socket_last_error($socket)));
				
				break;
			}
			
			echo $buffer."\n";
			log_info("socket_read:{$buffer}");
			break;
			
		} while (true);
				
		if (strlen($buffer) > 0) {
			$buff_list = explode(" ", $buffer);
			if ($buff_list[0] == 'SENDNOTICETOOTHERPROGRESS') {
				if ($buff_list[1] == 'OK') {
					socket_close($socket);
					return true;
				}	
			}	
		}
		
		socket_close($socket);
		
		return false;
		
	} catch (PDOException $e) {
		log_error("send socket to tuid:{$tuid} fail:". $e->getMessage());
		
		socket_close($socket);
		return false;
	}
}

// return bool
function send_notice_to_uid($fuid, $fnick, $tuid, $tag_type)
{
	$ret = false;
	
	// 检查收件人是否在线
	$tpid = mc_get($tuid);
	if ($tpid != false) {
		// 用户在线，使用Socket给用户发送通知
		$ret = send_notice_with_socket($tpid, $fuid, $fnick, $tuid, $tag_type);
		if ($ret == true) {
			return $ret;
		}
	} 
	
	// 用户不在线，使用APNS发送通知
	$ret = send_notice_with_apns($fuid, $fnick, $tuid, $tag_type);
	return $ret;
}


// return :
// false: bool
// succ: result
function mc_get($key)
{
	$mc = new Memcache;
	$ret = $mc->connect(MC_HOST, MC_PORT);
	if ($ret == FALSE) {
		return $ret;
	}
	$val = $mc->get($key);
	$mc->close();

	return $val;
}
// return bool
function mc_set($key, $val, $expire)
{
	$mc = new Memcache;
	$mc->connect(MC_HOST, MC_PORT);
	$ret = $mc->set($key, $val, 0, $expire);
	$mc->close();

	return $ret;
}
// 返回 bool
function mc_delete($key)
{
	$mc = new Memcache;
	$mc->connect(MC_HOST, MC_PORT);
	$ret = $mc->delete($key);
	$mc->close();

	return $ret;
}

?>
