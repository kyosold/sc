<?php

include_once('common.php');
include_once('utils.php');

$qid = trim(avoid_sql($_POST['qid']));
$mid = trim(avoid_sql($_POST['mid']));
$action = trim(avoid_sql($_POST['action']));

$res = array();

if (strlen($qid) > 0) {
	$fuid = 0;
	$fnick = '';
	$tuid = 0;
	$tnick = 0;
	$tag_type = '';
	$queue_type = '';
	$expire = 0;

	 try {   
        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
            
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		// 检查是否有请求记录
		$sql = "SELECT id, mid, tag_type, fuid, fnick, tuid, queue_type, queue_file FROM sc_queue WHERE id = {$qid} AND expire = 0 LIMIT 1";
		$stmt = $db->prepare($sql);
        $stmt->execute();
        if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$tuid = $row['tuid']; 
			$fuid = $row['fuid']; 
			$fnick = $row['fnick'];
			$tag_type = $row['tag_type'];	
			$queue_type = $row['queue_type'];
			$expire = 0;
						
		} else {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');	
		}

		// 获取收件人昵称
		$sql = "SELECT nickname FROM sc_user WHERE id = {$tuid} LIMIT 1";
		$stmt = $db->prepare($sql);
        $stmt->execute();
        if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
        	$tnick = base64_encode($row['nickname']);
        }   

		if ($fuid == 0 || $tuid == 0 || $fnick == '' || $tnick == '') {
			$res = show_info('fail', '统出错(4005)，请稍候重试 :)');
			echo json_encode($res);
			return 0;
		}

		$need_notice = false;

		// ----- 开始事务 -----
		$db->beginTransaction();

		// 更新请求记录为无效
		$sql = "UPDATE sc_queue SET expire = 1 WHERE id = {$qid} LIMIT 1";
		$stmt = $db->prepare($sql);
		if (!$stmt->execute()) {
			throw new PDOException('系统出错(2001)，请稍候重试 :)');
		}

		if ($action == "add") {
			// 写数据到关系表
			$sql = "INSERT INTO sc_relationship (myid, fid, pid, status) VALUES ";
			$sql .= "({$fuid}, {$tuid}, 0, 0), ({$tuid}, {$fuid}, 0, 0);";
			$stmt = $db->prepare($sql);
			if (!$stmt->execute()) {
				throw new PDOException('系统出错(2001)，请稍候重试 :)');
			}
			$affected_rows = $stmt->rowCount();
			if ($affected_rows != 2) {
				throw new PDOException('系统出错(2001)，请稍候重试 :)');
			}

			// 添加同意记录
			$sql = "INSERT INTO sc_queue (mid, tag_type, fuid, fnick, tuid, queue_type, queue_file, queue_size, image_wh, expire, fdel, tdel) VALUES ";
			$sql .= "({$mid}, 'SYS', {$tuid}, '{$tnick}', {$fuid}, 'cmf', 'agree', 5, '', 0, 0, 0)";
			$stmt = $db->prepare($sql);
			if (!$stmt->execute()) {
				throw new PDOException('系统出错(2001)，请稍候重试 :)');
			}
			$affected_rows = $stmt->rowCount();
			if ($affected_rows != 1) {
				throw new PDOException('系统出错(2001)，请稍候重试 :)');
			}
			
			$need_notice = true;
		
		} else if ($action == "cancel" || $action == "reject") {
		}

		// ----- 完成事务 -----
		$db->commit();


		$need_notice = true;
		if ($need_notice == true) {

			// 发送一个通知
			$ret = send_notice_to_uid($tuid, $tnick, $fuid, $tag_type);
			if ($ret == true) {
				$res = show_info('succ', '处理成功');
			} else {
				$res = show_info('succ', '处理成功');
			}
			echo json_encode($res);

			log_info("set friend action: send_notice_to_uid {$ret}");

			return 0;
			
		} else {
			log_info("set friend action fail: 请选择执行的动作");
			$res = show_info('fail', '请选择执行的动作');
			echo json_encode($res);
			return 0;
		}
	
	} catch (PDOException $e) {
		$db->rollback();
		
		log_info("set friend action fail, ".$e->getMessage());
		$res = show_info('fail', $e->getMessage());
        $res['localdes'] = $e->getMessage();
        echo json_encode($res);
		return 1;
	}		

    return 0;

} else {
	log_info("set friend action fail: parameters error");
	$res['status'] = 'fail';
	$res['des'] = 'parameters error';
}




echo json_encode($res);

?>

