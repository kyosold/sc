<?php

include_once('common.php');
include_once('utils.php');

$uid = trim(avoid_sql($_POST['uid']));
$tuid = trim(avoid_sql($_POST['tuid']));

$res = array();

if (strlen($uid) > 0 &&
	strlen($tuid) > 0) {


    try {
        $db = new PDO($PDO_DB_DSN, DB_USER, DB_PWD);
    
        $db->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER); //设置属性
        $db->setAttribute(PDO::ATTR_ERRMODE,PDO::ERRMODE_EXCEPTION);

		// 查询关系表
		$sql = "SELECT id, pid, status FROM sc_relationship WHERE myid = {$uid} AND fid = {$tuid} LIMIT 1";
		$stmt = $db->prepare($sql);
		$stmt->execute();
		if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$res = show_info('fail', '对方已经是你的好友了');
			echo json_encode($res);
			return 0;	
		}


		// 查询队列表
		$sql = "SELECT id FROM sc_queue WHERE fuid = {$uid} AND tuid = {$tuid} AND tag_type = 'SYS' AND queue_type = 'rmf' AND expire = 0 LIMIT 1";
		$stmt = $db->prepare($sql);
        $stmt->execute();
        if ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
			$res = show_info('fail', '请求已经发送');
			echo json_encode($res);
			return 0;   	
		}

		$res = show_info('succ', '正常,可以继续发送请求!');
		$res['sql'] = $sql;

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
