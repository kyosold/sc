<?php

include_once('common.php');
include_once('utils.php');

$mid = trim(avoid_sql($_POST['mid']));
$fuid = trim(avoid_sql($_POST['fuid']));
$fnick = trim(avoid_sql($_POST['fnick']));
$tuid = trim(avoid_sql($_POST['tuid']));
$content = trim(avoid_sql($_POST['content']));

$tag_type = "CHAT";

$res = array();


if (strlen($mid) > 0 &&
	strlen($fuid) > 0 &&
	strlen($fnick) > 0 &&
	strlen($tuid) > 0) {

    try {

        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		$tpid = 0;

		// ----- 查询是否已经是好友 -----
		// 查询关系表
		$sql = "SELECT id, pid, status FROM sc_relationship WHERE myid = {$tuid} AND fid = {$fuid} LIMIT 1";
		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$tpid = $row['pid'];
		} else {
			$res = show_info('fail', '对方不是你的好友');
			echo json_encode($res);
			return 0;	
		}


		// ----- 操作 -----
		// 1. 写消息到文件队列
		$file = write_content_to_file_with_uid($tuid, $content);
		if (strlen($file) <= 0) {
			$res = show_info('fail', '系统出错');
			echo json_encode($res);
			return;
		}
		
		// 1. 写消息到数据库
		$fnickB64 = base64_encode($fnick);
		$qid = write_queue_to_db($db, $mid, $tag_type, $fuid, $fnickB64, $tuid, "txt", $file, strlen($content), "");
		if ($qid == -1) {
			$res = show_info('fail', '系统出错');
			echo json_encode($res);
			return;
		}

		// 2. 给对方发送通知 
		$ret = send_notice_to_uid($fuid, $fnickB64, $tuid, $tpid, $tag_type);
		if ($ret != true) {
			log_info("send notice to tuid:{$tuid} tag_type:{$tag_type} fail");
        }   

		$res = show_info('succ', '处理成功');
		$res['qid'] = $qid;
        echo json_encode($res);
		return;

	} catch (PDOException $e) {
		log_info("find user fail: ".$e->getMessage());

        $res = show_info('fail', $e->getMessage());
        echo json_encode($res);
        return 1;
	}



}

$res['status'] = 'fail';
$res['des'] = 'parameters error';

echo json_encode($res);



?>
