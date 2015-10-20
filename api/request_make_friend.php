<?php

include_once('common.php');
include_once('utils.php');

$mid = trim(avoid_sql($_POST['mid']));
$fuid = trim(avoid_sql($_POST['fuid']));
$fnick = trim(avoid_sql($_POST['fnick']));
$tuid = trim(avoid_sql($_POST['tuid']));

$tag_type = "SYS";

$res = array();


if (strlen($mid) > 0 &&
	strlen($fuid) > 0 &&
	strlen($fnick) > 0 &&
	strlen($tuid) > 0) {

    try {

        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		// ----- 查询是否已经是好友 -----
		// 查询关系表
		$sql = "SELECT id, pid, status FROM sc_relationship WHERE myid = {$fuid} AND fid = {$tuid} LIMIT 1";
		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$res = show_info('fail', '对方已经是你的好友了');
			echo json_encode($res);
			return 0;	
		}


		// 查询队列表
		$sql = "SELECT id FROM sc_queue WHERE fuid = {$fuid} AND tuid = {$tuid} AND tag_type = 'SYS' AND queue_type = 'rmf' AND expire = 0 LIMIT 1";
		$stmt = $db->prepare($sql);
        $stmt->execute();
        if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$res = show_info('fail', '请求已经发送');
			echo json_encode($res);
			return 0;   	
		}


		// ----- 操作 -----
		// 1. 写消息到数据库
		$fnickB64 = base64_encode($fnick);
		$pid = write_queue_to_db($db, $mid, $tag_type, $fuid, $fnickB64, $tuid, "rmf", "", 0, "");
		if ($pid == -1) {
			$res = show_info('fail', '系统出错');
			echo json_encode($res);
			return;
		}

		// 2. 给对方发送通知 
		$ret = send_notice_to_uid($fuid, $fnickB64, $tuid, $tag_type);
		if ($ret != true) {
			log_info("send notice to tuid:{$tuid} tag_type:{$tag_type} fail");
        }   

		$res = show_info('succ', '处理成功');
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
