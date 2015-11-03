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


function write_content_to_file_with_uid($tuid, $content) 
{
	// 生成目录
	$path = DATA_PATH ."/". $tuid ."/queue/";
	$file = "";
	$succ = FALSE;

	// hash目录
	$path .= date('Ymd')."/";
	
	if (!file_exists($path)) {
		// 创建目录
		if (mk_dir($path) !== TRUE) {
			log_error('fail', 'mkdir {$path} fail');
			return "";
		}
	}
	
	$seq = 0;
	for ($i=0; $i<100; $i++,$seq++) {
		$file = $path ."/". time() .".". $seq;
		
		if (file_exists($file)) {
			continue;
		}
		
		$succ = file_put_contents($file, $content);
		if ($succ === FALSE) {
			continue;
		} else {
			break;
		}
		 
	}
	
	$file = str_replace(DATA_PATH, DATA_HOST, $file);

	return $file;
}


// 返回:
//	-1: fail	other: pid
function write_queue_to_db($db, $mid, $tag_type, $fuid, $fnick, $tuid, $queue_type, $queue_file, $queue_size, $image_wh)
{
	try {
		$sql = "INSERT INTO sc_queue (mid, tag_type, fuid, fnick, tuid, queue_type, queue_file, queue_size, image_wh, expire) VALUES ";
		$sql .= "('{$mid}', '{$tag_type}', {$fuid}, '{$fnick}', {$tuid}, '{$queue_type}', '{$queue_file}', {$queue_size}, '{$image_wh}', 0)";
		
		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException($sql);
		}

		$pid = $db->lastInsertId();

		return $pid;

	} catch (PDOException $e) {
		log_error("write queue to db fail:". $e->getMessage());
		
		return -1;
	}
}



// return bool
function send_notice_with_apns($qid, $fuid, $fnick, $tuid, $tag_type)
{
	// 获取收件人的ios_token
	$ios_token = '';
	try {
		global $PDO_DB_DSN;

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

		if (strlen($ios_token) <= 0 ) {
			log_info("user ios_token is not valid, so don't apns");
			return false;
		}

		
		// 发送apns
		$apn_res = '';
		$apn_cmd = APNS_URI." '{$ios_token}' '{$qid}' '{$fuid}' '{$fnick}' '{$tag_type}' '{$tuid}' ";

		$pipe = popen($apn_cmd, "r");
		while (!feof($pipe)) {
			$apn_res .= fgets($pipe);
		}

		pclose($pipe);
		
		$apn_res = str_replace("\n", " LF ", $apn_res);
		log_info("APNS result:{$apn_res}");
		
		return true;
		
	} catch (PDOException $e) {
		log_error("send apns to tuid:{$tuid} fail:". $e->getMessage());
		
		return false;
	}
}

function send_notice_with_socket($tpid, $mqid, $fuid, $fnick, $tuid, $tag_type)
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
		$wbuf = "SENDNOTICETOOTHERPROGRESS pid:{$tpid} mqid:{$mqid} fuid:{$fuid} fnick:{$fnick} mtype:{$tag_type} tuid:{$tuid} \r\n\r\n";
		log_info("wbuf:{$wbuf}");
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
			
			//echo $buffer."\n";
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
function send_notice_to_uid($fuid, $fnick, $tuid, $tpid, $tag_type, $qid)
{
	$ret = false;
	
	// 检查收件人是否在线
	$tpid = mc_get($tuid);
	if ($tpid != false && strlen($tpid) > 0) {
		// 用户在线，使用Socket给用户发送通知
		log_info("send_notice_with_socket qid:{$qid}");
		$ret = send_notice_with_socket($tpid, $qid, $fuid, $fnick, $tuid, $tag_type);
		if ($ret == true) {
			return $ret;
		}
	} 
	
	// 用户不在线，使用APNS发送通知

	// 判断是否可以使用apns推送
	if ($tpid > 0) {
		// 有风险,可能发件人被收件人设置隐藏
		log_info("don't send APNS, maybe receiver set hidden to sender");
		return 0;
	}

	$ret = send_notice_with_apns($qid, $fuid, $fnick, $tuid, $tag_type);
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
